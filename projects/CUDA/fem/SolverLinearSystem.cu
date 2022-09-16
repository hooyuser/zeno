#include "../Utils.hpp"
#include "Solver.cuh"
#include "zensim/geometry/Distance.hpp"
#include "zensim/geometry/Friction.hpp"
#include "zensim/geometry/SpatialQuery.hpp"
#include "zensim/types/SmallVector.hpp"

namespace zeno {

template <typename VecT, int N = VecT::template range_t<0>::value,
          zs::enable_if_all<N % 3 == 0, N == VecT::template range_t<1>::value> = 0>
__forceinline__ __device__ void rotate_hessian(zs::VecInterface<VecT> &H, const typename IPCSystem::mat3 BCbasis[N / 3],
                                               const int BCorder[N / 3], const int BCfixed[], bool projectDBC) {
    // hessian rotation: trans^T hess * trans
    // left trans^T: multiplied on rows
    // right trans: multiplied on cols
    constexpr int NV = N / 3;
    // rotate and project
    for (int vi = 0; vi != NV; ++vi) {
        int offsetI = vi * 3;
        for (int vj = 0; vj != NV; ++vj) {
            int offsetJ = vj * 3;
            IPCSystem::mat3 tmp{};
            for (int i = 0; i != 3; ++i)
                for (int j = 0; j != 3; ++j)
                    tmp(i, j) = H(offsetI + i, offsetJ + j);
            // rotate
            tmp = BCbasis[vi].transpose() * tmp * BCbasis[vj];
            // project
            if (projectDBC) {
                for (int i = 0; i != 3; ++i) {
                    bool clearRow = i < BCorder[vi];
                    for (int j = 0; j != 3; ++j) {
                        bool clearCol = j < BCorder[vj];
                        if (clearRow || clearCol)
                            tmp(i, j) = (vi == vj && i == j ? 1 : 0);
                    }
                }
            } else {
                for (int i = 0; i != 3; ++i) {
                    bool clearRow = i < BCorder[vi] && BCfixed[vi] == 1;
                    for (int j = 0; j != 3; ++j) {
                        bool clearCol = j < BCorder[vj] && BCfixed[vj] == 1;
                        if (clearRow || clearCol)
                            tmp(i, j) = (vi == vj && i == j ? 1 : 0);
                    }
                }
            }
            for (int i = 0; i != 3; ++i)
                for (int j = 0; j != 3; ++j)
                    H(offsetI + i, offsetJ + j) = tmp(i, j);
        }
    }
    return;
}

/// inertia
void IPCSystem::computeInertialAndGravityPotentialGradient(zs::CudaExecutionPolicy &cudaPol) {
    using namespace zs;
    constexpr auto space = execspace_e::cuda;
    // inertial
    cudaPol(zs::range(coOffset), [tempI = proxy<space>({}, tempI), vtemp = proxy<space>({}, vtemp), dt = dt,
                                  projectDBC = projectDBC] ZS_LAMBDA(int i) mutable {
        auto m = zs::sqr(vtemp("ws", i));
        vtemp.tuple<3>("grad", i) =
            vtemp.pack<3>("grad", i) - m * (vtemp.pack<3>("xn", i) - vtemp.pack<3>("xtilde", i));

        auto M = mat3::identity() * m;
        mat3 BCbasis[1] = {vtemp.template pack<3, 3>("BCbasis", i)};
        int BCorder[1] = {(int)vtemp("BCorder", i)};
        int BCfixed[1] = {(int)vtemp("BCfixed", i)};
        rotate_hessian(M, BCbasis, BCorder, BCfixed, projectDBC);
        tempI.template tuple<9>("Hi", i) = M;
        // prepare preconditioner
        for (int r = 0; r != 3; ++r)
            for (int c = 0; c != 3; ++c)
                vtemp("P", r * 3 + c, i) += M(r, c);
    });
    // extforce (only grad modified)
    for (auto &primHandle : prims) {
        if (primHandle.isBoundary()) // skip soft boundary
            continue;
        cudaPol(zs::range(primHandle.getVerts().size()), [vtemp = proxy<space>({}, vtemp), extForce = extForce, dt = dt,
                                                          vOffset = primHandle.vOffset] ZS_LAMBDA(int vi) mutable {
            auto m = zs::sqr(vtemp("ws", vOffset + vi));
            int BCorder = vtemp("BCorder", vOffset + vi);
            int BCsoft = vtemp("BCsoft", vOffset + vi);
            if (BCsoft == 0 && BCorder != 3)
                vtemp.tuple<3>("grad", vOffset + vi) = vtemp.pack<3>("grad", vOffset + vi) + m * extForce * dt * dt;
        });
    }
    if (vtemp.hasProperty("extf")) {
        cudaPol(zs::range(coOffset), [vtemp = proxy<space>({}, vtemp), dt = dt] ZS_LAMBDA(int vi) mutable {
            int BCorder = vtemp("BCorder", vi);
            int BCsoft = vtemp("BCsoft", vi);
            if (BCsoft == 0 && BCorder != 3)
                vtemp.template tuple<3>("grad", vi) =
                    vtemp.template pack<3>("grad", vi) + vtemp.template pack<3>("extf", vi) * dt * dt;
        });
    }
}
void IPCSystem::computeInertialPotentialGradient(zs::CudaExecutionPolicy &cudaPol, const zs::SmallString &gTag) {
    using namespace zs;
    constexpr auto space = execspace_e::cuda;
    // inertial
    cudaPol(zs::range(coOffset), [vtemp = proxy<space>({}, vtemp), gTag, dt = dt] ZS_LAMBDA(int i) mutable {
        auto m = zs::sqr(vtemp("ws", i));
        vtemp.tuple<3>(gTag, i) = vtemp.pack<3>(gTag, i) - m * (vtemp.pack<3>("xn", i) - vtemp.pack<3>("xtilde", i));
    });
}

/// elasticity
template <typename Model>
void computeElasticGradientAndHessianImpl(zs::CudaExecutionPolicy &cudaPol, const zs::SmallString &gTag,
                                          typename IPCSystem::dtiles_t &vtemp,
                                          typename IPCSystem::PrimitiveHandle &primHandle, const Model &model,
                                          typename IPCSystem::T dt, bool projectDBC, bool includeHessian) {
    using namespace zs;
    constexpr auto space = execspace_e::cuda;
    using mat3 = typename IPCSystem::mat3;
    using vec3 = typename IPCSystem::vec3;
    using T = typename IPCSystem::T;
    if (primHandle.category == ZenoParticles::curve) {
        if (primHandle.isBoundary() && !primHandle.isAuxiliary())
            return;
        /// ref: Fast Simulation of Mass-Spring Systems
        /// credits: Tiantian Liu
        cudaPol(zs::range(primHandle.getEles().size()),
                [vtemp = proxy<space>({}, vtemp), etemp = proxy<space>({}, primHandle.etemp),
                 eles = proxy<space>({}, primHandle.getEles()), model, gTag, dt = dt, projectDBC = projectDBC,
                 vOffset = primHandle.vOffset, includeHessian,
                 n = primHandle.getEles().size()] __device__(int ei) mutable {
                    auto inds = eles.template pack<2>("inds", ei).template reinterpret_bits<int>() + vOffset;
                    mat3 BCbasis[2];
                    int BCorder[2];
                    int BCfixed[2];
                    for (int i = 0; i != 2; ++i) {
                        BCbasis[i] = vtemp.pack<3, 3>("BCbasis", inds[i]);
                        BCorder[i] = vtemp("BCorder", inds[i]);
                        BCfixed[i] = vtemp("BCfixed", inds[i]);
                    }

                    if (BCorder[0] == 3 && BCorder[1] == 3) {
                        etemp.tuple<6 * 6>("He", ei) = zs::vec<T, 6, 6>::zeros();
                        return;
                    }

                    auto vole = eles("vol", ei);
                    auto k = eles("k", ei);
                    auto rl = eles("rl", ei);

                    vec3 xs[2] = {vtemp.template pack<3>("xn", inds[0]), vtemp.template pack<3>("xn", inds[1])};
                    auto xij = xs[1] - xs[0];
                    auto lij = xij.norm();
                    auto dij = xij / lij;
                    auto gij = k * (lij - rl) * dij;

                    // gradient
                    auto vfdt2 = gij * (dt * dt) * vole;
                    for (int d = 0; d != 3; ++d) {
                        atomic_add(exec_cuda, &vtemp(gTag, d, inds[0]), (T)vfdt2(d));
                        atomic_add(exec_cuda, &vtemp(gTag, d, inds[1]), (T)-vfdt2(d));
                    }

                    if (!includeHessian)
                        return;
                    auto H = zs::vec<T, 6, 6>::zeros();
                    auto K = k * (mat3::identity() - rl / lij * (mat3::identity() - dyadic_prod(dij, dij)));
                    // make_pd(K);  // symmetric semi-definite positive, not
                    // necessary

                    for (int i = 0; i != 3; ++i)
                        for (int j = 0; j != 3; ++j) {
                            H(i, j) = K(i, j);
                            H(i, 3 + j) = -K(i, j);
                            H(3 + i, j) = -K(i, j);
                            H(3 + i, 3 + j) = K(i, j);
                        }
                    H *= dt * dt * vole;

                    // rotate and project
                    rotate_hessian(H, BCbasis, BCorder, BCfixed, projectDBC);
                    etemp.tuple<6 * 6>("He", ei) = H;
                    for (int vi = 0; vi != 2; ++vi) {
                        for (int i = 0; i != 3; ++i)
                            for (int j = 0; j != 3; ++j) {
                                atomic_add(exec_cuda, &vtemp("P", i * 3 + j, inds[vi]), H(vi * 3 + i, vi * 3 + j));
                            }
                    }
                });
    } else if (primHandle.category == ZenoParticles::surface) {
        if (primHandle.isBoundary())
            return;
        cudaPol(zs::range(primHandle.getEles().size()),
                [vtemp = proxy<space>({}, vtemp), etemp = proxy<space>({}, primHandle.etemp),
                 eles = proxy<space>({}, primHandle.getEles()), model, gTag, dt = dt, projectDBC = projectDBC,
                 vOffset = primHandle.vOffset, includeHessian] __device__(int ei) mutable {
                    auto IB = eles.template pack<2, 2>("IB", ei);
                    auto inds = eles.template pack<3>("inds", ei).template reinterpret_bits<int>() + vOffset;
                    auto vole = eles("vol", ei);
                    vec3 xs[3] = {vtemp.template pack<3>("xn", inds[0]), vtemp.template pack<3>("xn", inds[1]),
                                  vtemp.template pack<3>("xn", inds[2])};
                    auto x1x0 = xs[1] - xs[0];
                    auto x2x0 = xs[2] - xs[0];

                    mat3 BCbasis[3];
                    int BCorder[3];
                    int BCfixed[3];
                    for (int i = 0; i != 3; ++i) {
                        BCbasis[i] = vtemp.pack<3, 3>("BCbasis", inds[i]);
                        BCorder[i] = vtemp("BCorder", inds[i]);
                        BCfixed[i] = vtemp("BCfixed", inds[i]);
                    }
                    zs::vec<T, 9, 9> H;
                    if (BCorder[0] == 3 && BCorder[1] == 3 && BCorder[2] == 3) {
                        etemp.tuple<9 * 9>("He", ei) = H.zeros();
                        return;
                    }

                    zs::vec<T, 3, 2> Ds{x1x0[0], x2x0[0], x1x0[1], x2x0[1], x1x0[2], x2x0[2]};
                    auto F = Ds * IB;

                    auto dFdX = dFdXMatrix(IB, wrapv<3>{});
                    auto dFdXT = dFdX.transpose();
                    auto f0 = col(F, 0);
                    auto f1 = col(F, 1);
                    auto f0Norm = zs::sqrt(f0.l2NormSqr());
                    auto f1Norm = zs::sqrt(f1.l2NormSqr());
                    auto f0Tf1 = f0.dot(f1);
                    zs::vec<T, 3, 2> Pstretch, Pshear;
                    for (int d = 0; d != 3; ++d) {
                        Pstretch(d, 0) = 2 * (1 - 1 / f0Norm) * F(d, 0);
                        Pstretch(d, 1) = 2 * (1 - 1 / f1Norm) * F(d, 1);
                        Pshear(d, 0) = 2 * f0Tf1 * f1(d);
                        Pshear(d, 1) = 2 * f0Tf1 * f0(d);
                    }
                    auto vecP = flatten(model.mu * Pstretch + (model.mu * 0.3) * Pshear);
                    auto vfdt2 = -vole * (dFdXT * vecP) * (dt * dt);

                    for (int i = 0; i != 3; ++i) {
                        auto vi = inds[i];
                        for (int d = 0; d != 3; ++d)
                            atomic_add(exec_cuda, &vtemp(gTag, d, vi), (T)vfdt2(i * 3 + d));
                    }

                    if (!includeHessian)
                        return;
                    /// ref: A Finite Element Formulation of Baraff-Witkin Cloth
                    // suggested by huang kemeng
                    auto stretchHessian = [&F, &model]() {
                        auto H = zs::vec<T, 6, 6>::zeros();
                        const zs::vec<T, 2> u{1, 0};
                        const zs::vec<T, 2> v{0, 1};
                        const T I5u = (F * u).l2NormSqr();
                        const T I5v = (F * v).l2NormSqr();
                        const T invSqrtI5u = (T)1 / zs::sqrt(I5u);
                        const T invSqrtI5v = (T)1 / zs::sqrt(I5v);

                        H(0, 0) = H(1, 1) = H(2, 2) = zs::max(1 - invSqrtI5u, (T)0);
                        H(3, 3) = H(4, 4) = H(5, 5) = zs::max(1 - invSqrtI5v, (T)0);

                        const auto fu = col(F, 0).normalized();
                        const T uCoeff = (1 - invSqrtI5u >= 0) ? invSqrtI5u : (T)1;
                        for (int i = 0; i != 3; ++i)
                            for (int j = 0; j != 3; ++j)
                                H(i, j) += uCoeff * fu(i) * fu(j);

                        const auto fv = col(F, 1).normalized();
                        const T vCoeff = (1 - invSqrtI5v >= 0) ? invSqrtI5v : (T)1;
                        for (int i = 0; i != 3; ++i)
                            for (int j = 0; j != 3; ++j)
                                H(3 + i, 3 + j) += vCoeff * fv(i) * fv(j);

                        H *= model.mu;
                        return H;
                    };
                    auto shearHessian = [&F, &model]() {
                        using mat6 = zs::vec<T, 6, 6>;
                        auto H = mat6::zeros();
                        const zs::vec<T, 2> u{1, 0};
                        const zs::vec<T, 2> v{0, 1};
                        const T I6 = (F * u).dot(F * v);
                        const T signI6 = I6 >= 0 ? 1 : -1;

                        H(3, 0) = H(4, 1) = H(5, 2) = H(0, 3) = H(1, 4) = H(2, 5) = (T)1;

                        const auto g_ = F * (dyadic_prod(u, v) + dyadic_prod(v, u));
                        zs::vec<T, 6> g{};
                        for (int j = 0, offset = 0; j != 2; ++j) {
                            for (int i = 0; i != 3; ++i)
                                g(offset++) = g_(i, j);
                        }

                        const T I2 = F.l2NormSqr();
                        const T lambda0 = (T)0.5 * (I2 + zs::sqrt(I2 * I2 + (T)12 * I6 * I6));

                        const zs::vec<T, 6> q0 = (I6 * H * g + lambda0 * g).normalized();

                        auto t = mat6::identity();
                        t = 0.5 * (t + signI6 * H);

                        const zs::vec<T, 6> Tq = t * q0;
                        const auto normTq = Tq.l2NormSqr();

                        mat6 dPdF =
                            zs::abs(I6) * (t - (dyadic_prod(Tq, Tq) / normTq)) + lambda0 * (dyadic_prod(q0, q0));
                        dPdF *= (model.mu * 0.3);
                        return dPdF;
                    };
                    auto He = stretchHessian() + shearHessian();
                    H = dFdX.transpose() * He * dFdX;
                    H *= dt * dt * vole;

                    // rotate and project
                    rotate_hessian(H, BCbasis, BCorder, BCfixed, projectDBC);
                    etemp.tuple<9 * 9>("He", ei) = H;
                    for (int vi = 0; vi != 3; ++vi) {
                        for (int i = 0; i != 3; ++i)
                            for (int j = 0; j != 3; ++j) {
                                atomic_add(exec_cuda, &vtemp("P", i * 3 + j, inds[vi]), H(vi * 3 + i, vi * 3 + j));
                            }
                    }
                });
    } else if (primHandle.category == ZenoParticles::tet)
        cudaPol(zs::range(primHandle.getEles().size()),
                [vtemp = proxy<space>({}, vtemp), etemp = proxy<space>({}, primHandle.etemp),
                 eles = proxy<space>({}, primHandle.getEles()), model, gTag, dt = dt, projectDBC = projectDBC,
                 vOffset = primHandle.vOffset, includeHessian] __device__(int ei) mutable {
                    auto IB = eles.template pack<3, 3>("IB", ei);
                    auto inds = eles.template pack<4>("inds", ei).template reinterpret_bits<int>() + vOffset;
                    auto vole = eles("vol", ei);
                    vec3 xs[4] = {vtemp.pack<3>("xn", inds[0]), vtemp.pack<3>("xn", inds[1]),
                                  vtemp.pack<3>("xn", inds[2]), vtemp.pack<3>("xn", inds[3])};

                    mat3 BCbasis[4];
                    int BCorder[4];
                    int BCfixed[4];
                    for (int i = 0; i != 4; ++i) {
                        BCbasis[i] = vtemp.pack<3, 3>("BCbasis", inds[i]);
                        BCorder[i] = vtemp("BCorder", inds[i]);
                        BCfixed[i] = vtemp("BCfixed", inds[i]);
                    }
                    zs::vec<T, 12, 12> H;
                    if (BCorder[0] == 3 && BCorder[1] == 3 && BCorder[2] == 3 && BCorder[3] == 3) {
                        etemp.tuple<12 * 12>("He", ei) = H.zeros();
                        return;
                    }
                    mat3 F{};
                    {
                        auto x1x0 = xs[1] - xs[0];
                        auto x2x0 = xs[2] - xs[0];
                        auto x3x0 = xs[3] - xs[0];
                        auto Ds = mat3{x1x0[0], x2x0[0], x3x0[0], x1x0[1], x2x0[1], x3x0[1], x1x0[2], x2x0[2], x3x0[2]};
                        F = Ds * IB;
                    }
                    auto P = model.first_piola(F);
                    auto vecP = flatten(P);
                    auto dFdX = dFdXMatrix(IB);
                    auto dFdXT = dFdX.transpose();
                    auto vfdt2 = -vole * (dFdXT * vecP) * dt * dt;

                    for (int i = 0; i != 4; ++i) {
                        auto vi = inds[i];
                        for (int d = 0; d != 3; ++d)
                            atomic_add(exec_cuda, &vtemp(gTag, d, vi), (T)vfdt2(i * 3 + d));
                    }

                    if (!includeHessian)
                        return;
                    auto Hq = model.first_piola_derivative(F, true_c);
                    H = dFdXT * Hq * dFdX * vole * dt * dt;

                    // rotate and project
                    rotate_hessian(H, BCbasis, BCorder, BCfixed, projectDBC);
                    etemp.tuple<12 * 12>("He", ei) = H;
                    for (int vi = 0; vi != 4; ++vi) {
                        for (int i = 0; i != 3; ++i)
                            for (int j = 0; j != 3; ++j) {
                                atomic_add(exec_cuda, &vtemp("P", i * 3 + j, inds[vi]), H(vi * 3 + i, vi * 3 + j));
                            }
                    }
                });
}

void IPCSystem::computeElasticGradientAndHessian(zs::CudaExecutionPolicy &cudaPol, const zs::SmallString &gTag,
                                                 bool includeHessian) {
    using namespace zs;
    constexpr auto space = execspace_e::cuda;
    for (auto &primHandle : prims) {
        match([&](auto &elasticModel) {
            computeElasticGradientAndHessianImpl(cudaPol, gTag, vtemp, primHandle, elasticModel, dt, projectDBC,
                                                 includeHessian);
        })(primHandle.getModels().getElasticModel());
    }
    for (auto &primHandle : auxPrims) {
        using ModelT = RM_CVREF_T(primHandle.getModels().getElasticModel());
        const ModelT &model = primHandle.modelsPtr ? primHandle.getModels().getElasticModel() : ModelT{};
        match([&](auto &elasticModel) {
            computeElasticGradientAndHessianImpl(cudaPol, gTag, vtemp, primHandle, elasticModel, dt, projectDBC,
                                                 includeHessian);
        })(model);
    }
}

void IPCSystem::computeBoundaryBarrierGradientAndHessian(zs::CudaExecutionPolicy &pol, bool includeHessian) {
    using namespace zs;
    constexpr auto space = execspace_e::cuda;
    for (auto &primHandle : prims) {
        if (primHandle.isBoundary()) // skip soft boundary
            continue;
        const auto &svs = primHandle.getSurfVerts();
        pol(range(svs.size()),
            [vtemp = proxy<space>({}, vtemp), svtemp = proxy<space>({}, primHandle.svtemp), svs = proxy<space>({}, svs),
             gn = s_groundNormal, dHat2 = dHat * dHat, kappa = kappa, projectDBC = projectDBC, includeHessian,
             svOffset = primHandle.svOffset] ZS_LAMBDA(int svi) mutable {
                const auto vi = reinterpret_bits<int>(svs("inds", svi)) + svOffset;
                auto x = vtemp.pack<3>("xn", vi);
                auto dist = gn.dot(x);
                auto dist2 = dist * dist;
                auto t = dist2 - dHat2;
                auto g_b = t * zs::log(dist2 / dHat2) * -2 - (t * t) / dist2;
                auto H_b = (zs::log(dist2 / dHat2) * -2.0 - t * 4.0 / dist2) + 1.0 / (dist2 * dist2) * (t * t);
                if (dist2 < dHat2) {
                    auto grad = -gn * (kappa * g_b * 2 * dist);
                    for (int d = 0; d != 3; ++d)
                        atomic_add(exec_cuda, &vtemp("grad", d, vi), grad(d));
                }

                if (!includeHessian)
                    return;
                auto param = 4 * H_b * dist2 + 2 * g_b;
                auto hess = mat3::zeros();
                if (dist2 < dHat2 && param > 0) {
                    auto nn = dyadic_prod(gn, gn);
                    hess = (kappa * param) * nn;
                }

                // make_pd(hess);
                mat3 BCbasis[1] = {vtemp.pack<3, 3>("BCbasis", vi)};
                int BCorder[1] = {(int)vtemp("BCorder", vi)};
                int BCfixed[1] = {(int)vtemp("BCfixed", vi)};
                rotate_hessian(hess, BCbasis, BCorder, BCfixed, projectDBC);
                svtemp.tuple<9>("H", svi) = hess;
                for (int i = 0; i != 3; ++i)
                    for (int j = 0; j != 3; ++j) {
                        atomic_add(exec_cuda, &vtemp("P", i * 3 + j, vi), hess(i, j));
                    }
            });

        if (s_enableFriction)
            if (fricMu != 0) {
                pol(range(svs.size()), [vtemp = proxy<space>({}, vtemp), svtemp = proxy<space>({}, primHandle.svtemp),
                                        svs = proxy<space>({}, svs), epsvh = epsv * dt, gn = s_groundNormal,
                                        fricMu = fricMu, projectDBC = projectDBC, includeHessian,
                                        svOffset = primHandle.svOffset] ZS_LAMBDA(int svi) mutable {
                    const auto vi = reinterpret_bits<int>(svs("inds", svi)) + svOffset;
                    auto dx = vtemp.pack<3>("xn", vi) - vtemp.pack<3>("xhat", vi);
                    auto fn = svtemp("fn", svi);
                    if (fn == 0) {
                        return;
                    }
                    auto coeff = fn * fricMu;
                    auto relDX = dx - gn.dot(dx) * gn;
                    auto relDXNorm2 = relDX.l2NormSqr();
                    auto relDXNorm = zs::sqrt(relDXNorm2);

                    vec3 grad{};
                    if (relDXNorm2 > epsvh * epsvh)
                        grad = -relDX * (coeff / relDXNorm);
                    else
                        grad = -relDX * (coeff / epsvh);
                    for (int d = 0; d != 3; ++d)
                        atomic_add(exec_cuda, &vtemp("grad", d, vi), grad(d));

                    if (!includeHessian)
                        return;

                    auto hess = mat3::zeros();
                    if (relDXNorm2 > epsvh * epsvh) {
                        zs::vec<T, 2, 2> mat{relDX[0] * relDX[0] * -coeff / relDXNorm2 / relDXNorm + coeff / relDXNorm,
                                             relDX[0] * relDX[2] * -coeff / relDXNorm2 / relDXNorm,
                                             relDX[0] * relDX[2] * -coeff / relDXNorm2 / relDXNorm,
                                             relDX[2] * relDX[2] * -coeff / relDXNorm2 / relDXNorm + coeff / relDXNorm};
                        make_pd(mat);
                        hess(0, 0) = mat(0, 0);
                        hess(0, 2) = mat(0, 1);
                        hess(2, 0) = mat(1, 0);
                        hess(2, 2) = mat(1, 1);
                    } else {
                        hess(0, 0) = coeff / epsvh;
                        hess(2, 2) = coeff / epsvh;
                    }

                    mat3 BCbasis[1] = {vtemp.pack<3, 3>("BCbasis", vi)};
                    int BCorder[1] = {(int)vtemp("BCorder", vi)};
                    int BCfixed[1] = {(int)vtemp("BCfixed", vi)};
                    rotate_hessian(hess, BCbasis, BCorder, BCfixed, projectDBC);
                    svtemp.template tuple<9>("H", svi) = svtemp.template pack<3, 3>("H", svi) + hess;
                    for (int i = 0; i != 3; ++i)
                        for (int j = 0; j != 3; ++j) {
                            atomic_add(exec_cuda, &vtemp("P", i * 3 + j, vi), hess(i, j));
                        }
                });
            }
    }
    return;
}

} // namespace zeno