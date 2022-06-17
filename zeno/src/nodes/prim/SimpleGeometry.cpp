#include <zeno/zeno.h>
#include <zeno/types/PrimitiveObject.h>
#include <zeno/types/DictObject.h>
#include <zeno/types/StringObject.h>
#include <zeno/types/PrimitiveTools.h>
#include <zeno/types/NumericObject.h>
#include <zeno/utils/string.h>
#include <zeno/utils/logger.h>
#include <zeno/utils/vec.h>
#define _USE_MATH_DEFINES
#include <math.h>
//#include <spdlog/spdlog.h>

namespace zeno {
struct CreateCube : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto size = get_input2<float>("size");
        auto position = get_input2<zeno::vec3f>("position");
        auto scaleSize = get_input2<zeno::vec3f>("scaleSize");

        auto &pos = prim->verts;
        pos.push_back(vec3f( 1,  1,  1) * size * scaleSize + position);
        pos.push_back(vec3f( 1,  1, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1,  1, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1,  1,  1) * size * scaleSize + position);
        pos.push_back(vec3f( 1, -1,  1) * size * scaleSize + position);
        pos.push_back(vec3f( 1, -1, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1, -1, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1, -1,  1) * size * scaleSize + position);

        auto &tris = prim->tris;
        // Top 0, 1, 2, 3
        tris.push_back(vec3i(0, 1, 2));
        tris.push_back(vec3i(0, 2, 3));
        // Right 0, 4, 5, 1
        tris.push_back(vec3i(0, 4, 5));
        tris.push_back(vec3i(0, 5, 1));
        // Front 0, 3, 7, 4
        tris.push_back(vec3i(0, 3, 7));
        tris.push_back(vec3i(0, 7, 4));
        // Left 2, 6, 7, 3
        tris.push_back(vec3i(2, 6, 7));
        tris.push_back(vec3i(2, 7, 3));
        // Back 1, 5, 6, 2
        tris.push_back(vec3i(1, 5, 6));
        tris.push_back(vec3i(1, 6, 2));
        // Bottom 4, 7, 6, 5
        tris.push_back(vec3i(4, 7, 6));
        tris.push_back(vec3i(4, 6, 5));
        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreateCube, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "size", "1"},
    },
    {"prim"},
    {},
    {"create"},
});

struct CreateCone : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto position = get_input2<zeno::vec3f>("position");
        auto scaleSize = get_input2<zeno::vec3f>("scaleSize");
        auto radius = get_input2<float>("radius");
        auto height = get_input2<float>("height");
        auto lons = get_input2<int>("lons");

        auto &pos = prim->verts;
        for (size_t i = 0; i < lons; i++) {
            float rad = 2 * M_PI * i / lons;
            pos.push_back(vec3f(cos(rad) * radius, -0.5 * height, -sin(rad) * radius) * scaleSize + position);
        }
        // top
        pos.push_back(vec3f(0, 0.5 * height, 0) * scaleSize + position);
        // bottom
        pos.push_back(vec3f(0, -0.5 * height, 0) * scaleSize + position);

        auto &tris = prim->tris;
        for (size_t i = 0; i < lons; i++) {
            tris.push_back(vec3i(lons, i, (i + 1) % lons));
            tris.push_back(vec3i(i, lons + 1, (i + 1) % lons));
        }

        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreateCone, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "radius", "1"},
        {"float", "height", "2"},
        {"int", "lons", "32"},
    },
    {"prim"},
    {},
    {"create"},
});

struct CreateDisk : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto position = get_input2<zeno::vec3f>("position");
        auto scaleSize = get_input2<zeno::vec3f>("scaleSize");
        auto radius = get_input2<float>("radius");
        auto lons = get_input2<int>("lons");

        auto &pos = prim->verts;
        for (size_t i = 0; i < lons; i++) {
            float rad = 2 * M_PI * i / lons;
            pos.push_back(vec3f(cos(rad) * radius, 0, -sin(rad) * radius) * scaleSize + position);
        }
        pos.push_back(vec3f(0, 0, 0) * scaleSize + position);

        auto &tris = prim->tris;
        for (size_t i = 0; i < lons; i++) {
            tris.push_back(vec3i(lons, i, (i + 1) % lons));
        }

        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreateDisk, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "radius", "1"},
        {"int", "lons", "32"},
    },
    {"prim"},
    {},
    {"create"},
});

struct CreatePlane : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto position = get_input2<zeno::vec3f>("position");
        auto scaleSize = get_input2<zeno::vec3f>("scaleSize");
        auto size = get_input2<float>("size");

        auto &pos = prim->verts;
        pos.push_back(vec3f( 1, 0,  1) * size * scaleSize + position);
        pos.push_back(vec3f( 1, 0, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1, 0, -1) * size * scaleSize + position);
        pos.push_back(vec3f(-1, 0,  1) * size * scaleSize + position);

        auto &tris = prim->tris;
        tris.push_back(vec3i(0, 1, 2));
        tris.push_back(vec3i(0, 2, 3));

        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreatePlane, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "size", "1"},
    },
    {"prim"},
    {},
    {"create"},
});

struct CreateCylinder : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();

        auto position = get_input2<zeno::vec3f>("position");
        auto scaleSize = get_input2<zeno::vec3f>("scaleSize");
        auto radius = get_input2<float>("radius");
        auto height = get_input2<float>("height");
        auto lons = get_input2<int>("lons");

        auto &pos = prim->verts;
        for (size_t i = 0; i < lons; i++) {
            float rad = 2 * M_PI * i / lons;
            pos.push_back(vec3f(cos(rad) * radius, 0.5 * height, -sin(rad) * radius) * scaleSize + position);
        }
        for (size_t i = 0; i < lons; i++) {
            float rad = 2 * M_PI * i / lons;
            pos.push_back(vec3f(cos(rad) * radius, -0.5 * height, -sin(rad) * radius) * scaleSize + position);
        }
        pos.push_back(vec3f(0, 0.5 * height, 0) * scaleSize + position);
        pos.push_back(vec3f(0, -0.5 * height, 0) * scaleSize + position);

        auto &tris = prim->tris;
        // Top
        for (size_t i = 0; i < lons; i++) {
            tris.push_back(vec3i(lons * 2, i, (i + 1) % lons));
        }
        // Bottom
        for (size_t i = 0; i < lons; i++) {
            tris.push_back(vec3i(i + lons, lons * 2 + 1, (i + 1) % lons + lons));
        }
        // Side
        for (size_t i = 0; i < lons; i++) {
            size_t _0 = i;
            size_t _1 = (i + 1) % lons;
            size_t _2 = (i + 1) % lons + lons;
            size_t _3 = i + lons;
            tris.push_back(vec3i(_1, _0, _2));
            tris.push_back(vec3i(_2, _0, _3));
        }
        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreateCylinder, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "radius", "1"},
        {"float", "height", "2"},
        {"int", "lons", "32"},
    },
    {"prim"},
    {},
    {"create"},
});

struct CreateSphere : zeno::INode {
    virtual void apply() override {
        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto position = get_input2<zeno::vec3f>("position");
        auto scale = get_input2<zeno::vec3f>("scaleSize");
        auto rows = get_input2<int>("rows");
        auto columns = get_input2<int>("columns");
        auto radius = get_input2<float>("radius");

        if(rows <= 3)
            rows = 3;
        if(columns <= 3)
            columns = 3;

        auto &uv = prim->add_attr<zeno::vec3f>("uv");
        auto &nrm = prim->add_attr<zeno::vec3f>("nrm");

        int c = 0;
        int tp = rows * columns;
        float row_sep = 180.0f / (rows - 1);

        for (int i = 0; i<rows; i++) {
            float ic = -90.0f + i*row_sep;
            float r = std::cos(ic / 180.0 * M_PI);
            float h = std::sin(ic / 180.0 * M_PI);

            for (int j = 0; j < columns; j++) {
                float rad = 2 * M_PI * j / columns;
                // position
                zeno::vec3f op = zeno::vec3f (
                    cos(rad) * r,
                    h,
                    sin(rad) * r);
                zeno::vec3f p = op * scale * radius + position;
                zeno::vec3f np = op * scale * radius;
                prim->verts.push_back(p);

                // normal
                zeno::vec3f n;
                n = zeno::normalize(np - zeno::vec3f(0,0,0));
                nrm.push_back(n);

                // uv
                zeno::vec3f uvw;
                float u,v;
                if(i == 0){
                    u = float(j)/(columns-1);
                    v = 0.0;

                }else if(i == rows-1){
                    u = float(j)/(columns-1);
                    v = 1.0;
                }else{
                    u = -1.0;
                    v = -1.0;
                }
                uv.emplace_back(u,v,0);
                if(j == 0 && i > 0 && i < rows-1){
                    prim->verts.push_back(p);
                    nrm.push_back(n);
                    uv.emplace_back(u,v,0);
                    c+=1;
                }

                // indices
                if(i == 0){
                    int i1 = c;//bottom
                    int i2 = c+columns+2;
                    int i3 = c+columns+1;
                    if(i2 >= 2*columns+1)
                        i2 = columns;
                    int i4 = tp-columns+j+rows-2;//top
                    int _t = tp-2*columns+rows-2;
                    int _t1 = tp-columns+rows-2;
                    int i5 = _t+j;
                    int i6 = _t+j+1;
                    if(i6>=_t1)
                        i6 -= columns+1;

                    prim->tris.push_back(zeno::vec3i(i1, i2, i3));
                    prim->tris.push_back(zeno::vec3i(i4, i5, i6));
                }

                if(rows > 3 && i < rows-2 && i>0){

                    int i1 = c;
                    int i2 = c+1;
                    int i3 = c+columns+1;

                    int i4 = i3;
                    int i5 = i1+columns+2;

                    if(j == columns-1){
                        i2 -= columns+1;
                        i5 -= columns+1;
                    }
                    int i6 = i2;

                    prim->tris.push_back(zeno::vec3i(i1, i2, i3));
                    prim->tris.push_back(zeno::vec3i(i4, i6, i5));
                }
                c+=1;
            }
        }

        float vi = 0;
        float s = 1.0f/(rows-1);
        for(int i=columns; i<(prim->verts.size() - columns); i++){

            int id = (i-columns)%(columns+1);
            if(id == 0)
                vi += 1;
            float u,v;
            if(id-1 < 0)
                u = 1.0;
            else
                u = float(id-1)/(columns);
            v = s*vi;
            uv[i] = zeno::vec3f(u,v,0);
        }

        set_output("prim", std::move(prim));
    }
};

ZENDEFNODE(CreateSphere, {
    {
        {"vec3f", "position", "0, 0, 0"},
        {"vec3f", "scaleSize", "1, 1, 1"},
        {"float", "radius", "1"},
        {"int", "rows", "13"},
        {"int", "columns", "24"},
    },
    {"prim"},
    {},
    {"create"},
});

}
