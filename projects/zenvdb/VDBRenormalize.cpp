#include <zeno/zeno.h>
#include <zeno/NumericObject.h>
#include <vector>
#include <zeno/VDBGrid.h>
#include <openvdb/points/PointCount.h>
#include <openvdb/tree/LeafManager.h>
#include <openvdb/points/PointAdvect.h>
#include <openvdb/tools/Morphology.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/LevelSetTracker.h>
#include <openvdb/tools/Filter.h>
#include <zeno/ParticlesObject.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/VolumeToSpheres.h>

namespace zeno {
    struct  VDBRenormalizeSDF : zeno::INode {
  virtual void apply() override {

    auto inoutSDF = get_input("inoutSDF")->as<VDBFloatGrid>();
    int normIter = std::get<int>(get_param("iterations"));
    int dilateIter = std::get<int>(get_param("dilateIters"));
    auto lstracker = openvdb::tools::LevelSetTracker<openvdb::FloatGrid>(*(inoutSDF->m_grid));
    lstracker.setState({openvdb::math::FIRST_BIAS, openvdb::math::TVD_RK3, 1, 1});
    lstracker.setTrimming(openvdb::tools::lstrack::TrimMode::kNone);

    if (dilateIter > 0)
        lstracker.dilate(dilateIter);
    else if (dilateIter < 0)
        lstracker.erode(dilateIter);
    for(int i=0;i<normIter;i++)
        lstracker.normalize();
    //openvdb::tools::changeBackground(inoutSDF->m_grid->tree(), ((float)normIter)*(inoutSDF->m_grid->transformPtr()->voxelSize()[0]));
    //openvdb::tools::signedFloodFill(inoutSDF->m_grid->tree());

    set_output("inoutSDF", get_input("inoutSDF"));
  }
};

static int defVDBRenormalizeSDF = zeno::defNodeClass<VDBRenormalizeSDF>("VDBRenormalizeSDF",
     { /* inputs: */ {
     "inoutSDF", 
     }, /* outputs: */ {
     "inoutSDF",
     }, /* params: */ {
         {"enum 1oUpwind", "method", "1oUpwind"},
         {"int", "iterations", "4"},
         {"int", "dilateIters", "0"},
     }, /* category: */ {
     "openvdb",
     }});

struct VDBSmooth : zeno::INode {
  virtual void apply() override {
    auto inoutVDBtype = get_input<VDBGrid>("inoutVDB")->getType();
    int width = get_input<NumericObject>("width")->get<int>();
    int iterations = get_input<NumericObject>("iterations")->get<int>();
    if (inoutVDBtype == std::string("FloatGrid")) {
        auto inoutVDB = get_input("inoutVDB")->as<VDBFloatGrid>();
        auto lsf = openvdb::tools::Filter<openvdb::FloatGrid>(*(inoutVDB->m_grid));
        lsf.setGrainSize(1);
        lsf.gaussian(width, iterations, nullptr);
        //openvdb::tools::ttls_internal::smoothLevelSet(*inoutSDF->m_grid, normIter, halfWidth);
        set_output("inoutVDB", get_input("inoutVDB"));
    }
    else if (inoutVDBtype == std::string("Vec3fGrid")) {
        auto inoutVDB = get_input("inoutVDB")->as<VDBFloat3Grid>();
        
        auto lsf = openvdb::tools::Filter<openvdb::Vec3fGrid>(*(inoutVDB->m_grid));
        lsf.setGrainSize(1);
        lsf.gaussian(width, iterations, nullptr);
        set_output("inoutVDB", get_input("inoutVDB"));
    }
  }
};

ZENO_DEFNODE(VDBSmooth)(
     { /* inputs: */ {
     "inoutVDB", 
     {"int", "width", "1"},
     {"int", "iterations", "1"},
     }, /* outputs: */ {
     "inoutVDB",
     }, /* params: */ {
         
     }, /* category: */ {
     "openvdb",
}});

struct  VDBDilateTopo : zeno::INode {
  virtual void apply() override {

    auto inoutSDF = get_input<zeno::VDBGrid>("inField");
    auto layers = get_input("layers")->as<zeno::NumericObject>()->get<int>();
    inoutSDF->dilateTopo(layers);
    set_output("oField", std::move(inoutSDF));
  }
};

static int defVDBDilateTopo = zeno::defNodeClass<VDBDilateTopo>("VDBDilateTopo",
     { /* inputs: */ {
     "inField", {"int", "layers",} 
     }, /* outputs: */ {
       "oField"
     }, /* params: */ {
     }, /* category: */ {
     "openvdb",
     }});

struct VDBErodeSDF : zeno::INode {
  virtual void apply() override {
    auto inoutSDF = get_input("inoutSDF")->as<VDBFloatGrid>();
    auto grid = inoutSDF->m_grid;
    auto depth = get_input("depth")->as<zeno::NumericObject>()->get<float>();
    auto wrangler = [&](auto &leaf, openvdb::Index leafpos) {
        for (auto iter = leaf.beginValueOn(); iter != leaf.endValueOn(); ++iter) {
            iter.modifyValue([&](auto &v) {
                v += depth;
            });
        }
    };
    auto velman = openvdb::tree::LeafManager<std::decay_t<decltype(grid->tree())>>(grid->tree());
    velman.foreach(wrangler);
    set_output("inoutSDF", get_input("inoutSDF"));
  }
};

static int defVDBErodeSDF = zeno::defNodeClass<VDBErodeSDF>("VDBErodeSDF",
     { /* inputs: */ {
     "inoutSDF", {"float", "depth"}, 
     }, /* outputs: */ {
       "inoutSDF",
     }, /* params: */ {
     }, /* category: */ {
     "openvdb",
     }});


struct ScalarFieldAnalyzer : zeno::INode {
    virtual void apply() override {
        auto inSDF = get_input("InVDB")->as<VDBFloatGrid>();
        auto grid = inSDF->m_grid;
        auto OpType = std::get<std::string>(get_param("Operator"));
        if (OpType == "Gradient") {
            auto result = std::make_shared<VDBFloat3Grid>(openvdb::tools::gradient(*grid));
            set_output("OutVDB", std::move(result));
        }
        else if (OpType == "Curvature") {
            auto result = std::make_shared<VDBFloatGrid>(openvdb::tools::meanCurvature(*grid));
            set_output("OutVDB", std::move(result));
        }
        else if (OpType == "Laplacian") {
            auto result = std::make_shared<VDBFloatGrid>(openvdb::tools::laplacian(*grid));
            set_output("OutVDB", std::move(result));
        }
        else {
            throw zeno::Exception("wrong parameter for ScalarFieldAnalyzer Operator: " + OpType);
        }  
    }
};

ZENO_DEFNODE(ScalarFieldAnalyzer)(
    { /* inputs: */ {
        "InVDB",   
    }, /* outputs: */ {
        "OutVDB"
    }, /* params: */ {
        {"enum Gradient Curvature Laplacian", "Operator", "Gradient"},
    }, /* category: */ {
        "openvdb",
    } });


struct VectorFieldAnalyzer : zeno::INode {
    virtual void apply() override {
        auto inSDF = get_input("InVDB")->as<VDBFloat3Grid>();
        auto grid = inSDF->m_grid;
        auto OpType = std::get<std::string>(get_param("Operator"));
        if (OpType == "Divergence") {
            auto result = std::make_shared<VDBFloatGrid>(openvdb::tools::divergence(*grid));
            set_output("OutVDB", std::move(result));
        }
        else if (OpType == "Curl") {
            auto result = std::make_shared<VDBFloat3Grid>(openvdb::tools::curl(*grid));
            set_output("OutVDB", std::move(result));
        }
        else if (OpType == "Magnitude") {
            auto result = std::make_shared<VDBFloatGrid>(openvdb::tools::magnitude(*grid));
            set_output("OutVDB", std::move(result));
        }
        else if (OpType == "Normalize") {
            auto result = std::make_shared<VDBFloat3Grid>(openvdb::tools::normalize(*grid));
            set_output("OutVDB", std::move(result));
        }
        else {
            throw zeno::Exception("wrong parameter for VectorFieldAnalyzer Operator: " + OpType);
        }
    }
};

ZENO_DEFNODE(VectorFieldAnalyzer)(
    { /* inputs: */ {
        "InVDB",
    }, /* outputs: */ {
        "OutVDB"
    }, /* params: */ {
        {"enum Divergence Curl Magnitude Normalize", "Operator", "Divergence"},
    }, /* category: */ {
        "openvdb",
    } });

}
