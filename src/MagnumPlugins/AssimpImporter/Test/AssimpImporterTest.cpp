/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017, 2020 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2018 Konstantinos Chatzilygeroudis <costashatz@gmail.com>
    Copyright © 2019, 2020 Max Schwarz <max.schwarz@ais.uni-bonn.de>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>

#include <assimp/defs.h> /* in assimp 3.0, version.h is missing this include for ASSIMP_API */
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Math::Literals;

struct AssimpImporterTest: TestSuite::Tester {
    explicit AssimpImporterTest();

    void openFile();
    void openFileFailed();
    void openData();
    void openDataFailed();

    void camera();
    void light();
    void lightUndefined();
    void materialColor();
    void materialTexture();
    void materialColorTexture();
    void materialStlWhiteAmbientPatch();
    void materialWhiteAmbientTexture();
    void materialMultipleTextures();
    void materialTextureCoordinateSetsDefault();
    void materialTextureCoordinateSets();

    void mesh();
    void pointMesh();
    void lineMesh();
    void meshMultiplePrimitives();

    void emptyCollada();
    void emptyGltf();
    void scene();
    void sceneCollapsedNode();
    void upDirectionPatching();
    void upDirectionPatchingPreTransformVertices();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNotFound();
    void imageExternalNoPathNoCallback();
    void imagePathMtlSpaceAtTheEnd();
    void imageMipLevels();

    void texture();

    void openState();
    void openStateTexture();

    void configurePostprocessFlipUVs();

    void fileCallback();
    void fileCallbackNotFound();
    void fileCallbackEmptyFile();
    void fileCallbackReset();
    void fileCallbackImage();
    void fileCallbackImageNotFound();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

constexpr struct {
    const char* name;
    ImporterFlags flags;
} VerboseData[]{
    {"", {}},
    {"verbose", ImporterFlag::Verbose}
};

constexpr struct {
    LightData::Type type;
    Color3 color;
} LightInstanceData[]{
    {LightData::Type::Spot, {0.12f, 0.24f, 0.36f}},
    {LightData::Type::Point, {0.5f, 0.25f, 0.05f}},
    {LightData::Type::Infinite, {1.0f, 0.15f, 0.45f}}
};

constexpr struct {
    const char* name;
    const char* file;
    bool importColladaIgnoreUpDirection;
    bool expectFail;
} UpDirectionPatchingData[]{
    {"Y up", "y-up.dae", false, false},
    {"Y up, ignored", "y-up.dae", true, false},
    {"Z up", "z-up.dae", false, false},
    {"Z up, ignored", "z-up.dae", true, true}
};

AssimpImporterTest::AssimpImporterTest() {
    addInstancedTests({&AssimpImporterTest::openFile},
        Containers::arraySize(VerboseData));

    addTests({&AssimpImporterTest::openFileFailed,
              &AssimpImporterTest::openData,
              &AssimpImporterTest::openDataFailed,

              &AssimpImporterTest::camera});

    addInstancedTests({&AssimpImporterTest::light},
        Containers::arraySize(LightInstanceData));

    addTests({&AssimpImporterTest::lightUndefined,
              &AssimpImporterTest::materialColor,
              &AssimpImporterTest::materialTexture,
              &AssimpImporterTest::materialColorTexture,
              &AssimpImporterTest::materialStlWhiteAmbientPatch,
              &AssimpImporterTest::materialWhiteAmbientTexture,
              &AssimpImporterTest::materialMultipleTextures,
              &AssimpImporterTest::materialTextureCoordinateSetsDefault,
              &AssimpImporterTest::materialTextureCoordinateSets,

              &AssimpImporterTest::mesh,
              &AssimpImporterTest::pointMesh,
              &AssimpImporterTest::lineMesh,
              &AssimpImporterTest::meshMultiplePrimitives,

              &AssimpImporterTest::emptyCollada,
              &AssimpImporterTest::emptyGltf,
              &AssimpImporterTest::scene,
              &AssimpImporterTest::sceneCollapsedNode});

    addInstancedTests({&AssimpImporterTest::upDirectionPatching,
                       &AssimpImporterTest::upDirectionPatchingPreTransformVertices},
        Containers::arraySize(UpDirectionPatchingData));

    addTests({&AssimpImporterTest::imageEmbedded,
              &AssimpImporterTest::imageExternal,
              &AssimpImporterTest::imageExternalNotFound,
              &AssimpImporterTest::imageExternalNoPathNoCallback,
              &AssimpImporterTest::imagePathMtlSpaceAtTheEnd,
              &AssimpImporterTest::imageMipLevels,

              &AssimpImporterTest::texture,

              &AssimpImporterTest::openState,
              &AssimpImporterTest::openStateTexture,

              &AssimpImporterTest::configurePostprocessFlipUVs,

              &AssimpImporterTest::fileCallback,
              &AssimpImporterTest::fileCallbackNotFound,
              &AssimpImporterTest::fileCallbackEmptyFile,
              &AssimpImporterTest::fileCallbackReset,
              &AssimpImporterTest::fileCallbackImage,
              &AssimpImporterTest::fileCallbackImageNotFound});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the
       filesystem. */
    #ifdef ASSIMPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ASSIMPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    /* The DdsImporter (for DDS loading / mip import tests) is optional */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The StbImageImporter (for PNG image loading) is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void AssimpImporterTest::openFile() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);

    std::ostringstream out;
    {
        Debug redirectOutput{&out};

        CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));
        CORRADE_VERIFY(importer->importerState());
        CORRADE_COMPARE(importer->sceneCount(), 1);
        CORRADE_COMPARE(importer->object3DCount(), 2);

        {
            /* https://github.com/assimp/assimp/blob/92078bc47c462d5b643aab3742a8864802263700/code/ColladaLoader.cpp#L225 */
            CORRADE_EXPECT_FAIL("Assimp adds some bogus skeleton visualizer mesh to COLLADA files that don't have any mesh.");
            CORRADE_VERIFY(!importer->meshCount());
        }

        importer->close();
        CORRADE_VERIFY(!importer->isOpened());
    }

    /* It should be noisy if and only if verbose output is enabled */
    Debug{Debug::Flag::NoNewlineAtTheEnd} << out.str();
    CORRADE_COMPARE(!out.str().empty(), data.flags >= ImporterFlag::Verbose);
}

void AssimpImporterTest::openFileFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile("i-do-not-exist.foo"));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open i-do-not-exist.foo: Unable to open file \"i-do-not-exist.foo\".\n");
}

void AssimpImporterTest::openData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    auto data = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"));
    CORRADE_VERIFY(importer->openData(data));
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    {
        /* https://github.com/assimp/assimp/blob/92078bc47c462d5b643aab3742a8864802263700/code/ColladaLoader.cpp#L225 */
        CORRADE_EXPECT_FAIL("Assimp adds some bogus skeleton visualizer mesh to COLLADA files that don't have any mesh.");
        CORRADE_VERIFY(!importer->meshCount());
    }

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::openDataFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};

    constexpr const char data[] = "what";
    CORRADE_VERIFY(!importer->openData({data, sizeof(data)}));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openData(): loading failed: No suitable reader found for the file format of file \"$$$___magic___$$$.\".\n");
}

void AssimpImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));

    CORRADE_COMPARE(importer->cameraCount(), 1);
    Containers::Optional<CameraData> camera = importer->camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 49.13434_degf);
    CORRADE_COMPARE(camera->near(), 0.123f);
    CORRADE_COMPARE(camera->far(), 123.0f);

    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Pointer<ObjectData3D> cameraObject = importer->object3D(0);
    CORRADE_COMPARE(cameraObject->instanceType(), ObjectInstanceType3D::Camera);
    CORRADE_COMPARE(cameraObject->instance(), 0);
}

void AssimpImporterTest::light() {
    auto&& data = LightInstanceData[testCaseInstanceId()];

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light.dae")));

    CORRADE_COMPARE(importer->lightCount(), 3);
    CORRADE_COMPARE(importer->object3DCount(), 3);

    Containers::Optional<LightData> light = importer->light(testCaseInstanceId());
    CORRADE_VERIFY(light);
    CORRADE_COMPARE(light->type(), data.type);
    CORRADE_COMPARE(light->color(), data.color);
    CORRADE_COMPARE(light->intensity(), 1.0f);

    Containers::Pointer<ObjectData3D> lightObject = importer->object3D(testCaseInstanceId());
    CORRADE_COMPARE(lightObject->instanceType(), ObjectInstanceType3D::Light);
    CORRADE_COMPARE(lightObject->instance(), testCaseInstanceId());
}

void AssimpImporterTest::lightUndefined() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light-undefined.dae")));

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp cannot load lights with undefined light type yet.");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->light(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): light type 4 is not supported\n");
}

void AssimpImporterTest::materialColor() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-color.dae")));

    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Pointer<AbstractMaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);

    PhongMaterialData* phongMaterial = static_cast<PhongMaterialData*>(material.get());
    CORRADE_VERIFY(phongMaterial);
    CORRADE_COMPARE(phongMaterial->flags(), PhongMaterialData::Flags{});
    {
        CORRADE_EXPECT_FAIL("Assimp sets ambient alpha to 0, ignoring the actual value (for COLLADA at least).");
        CORRADE_COMPARE(phongMaterial->ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 0.9f}));
    } {
        /* We're importing as Color3 instead, forcing the alpha to be 1 */
        CORRADE_COMPARE(phongMaterial->ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 1.0f}));
    }
    CORRADE_COMPARE(phongMaterial->diffuseColor(), (Color4{0.08f, 0.16f, 0.24f, 0.7f}));
    CORRADE_COMPARE(phongMaterial->specularColor(), (Color4{0.15f, 0.1f, 0.05f, 0.5f}));
    CORRADE_COMPARE(phongMaterial->shininess(), 50.0f);

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* Ancient assimp version add "-material" suffix */
    if(version < 302) {
        CORRADE_COMPARE(importer->materialForName("Material-material"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Material-material");
    } else {
        CORRADE_COMPARE(importer->materialForName("Material"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Material");
    }
    CORRADE_COMPARE(importer->materialForName("Ghost"), -1);
}

void AssimpImporterTest::materialTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Pointer<AbstractMaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);

    PhongMaterialData* phongMaterial = static_cast<PhongMaterialData*>(material.get());
    CORRADE_VERIFY(phongMaterial);

    {
        CORRADE_EXPECT_FAIL("Assimp ignores ambient textures in COLLADA files.");
        CORRADE_COMPARE(phongMaterial->flags(), PhongMaterialData::Flag::AmbientTexture|PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture|PhongMaterialData::Flag::NormalTexture);
        /* (This would assert now) */
        //CORRADE_COMPARE(phongMaterial->ambientTexture(), 1);
    } {
        CORRADE_COMPARE(phongMaterial->flags(), PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture|PhongMaterialData::Flag::NormalTexture);
    }
    CORRADE_COMPARE(importer->textureCount(), 3);
    CORRADE_COMPARE(phongMaterial->diffuseTexture(), 0);
    CORRADE_COMPARE(phongMaterial->specularTexture(), 1);
    CORRADE_COMPARE(phongMaterial->normalTexture(), 2);

    /* Colors should stay at their defaults as these aren't provided in the
       file */
    CORRADE_COMPARE(phongMaterial->ambientColor(), (Color4{0.0f, 0.0f, 0.0f, 1.0f}));
    CORRADE_COMPARE(phongMaterial->diffuseColor(), (Color4{1.0f, 1.0f, 1.0f, 1.0f}));
    CORRADE_COMPARE(phongMaterial->specularColor(), (Color4{1.0f, 1.0f, 1.0f, 1.0f}));
}

void AssimpImporterTest::materialColorTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-color-texture.obj")));

    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 1);
    } {
        CORRADE_COMPARE(importer->materialCount(), 2);
    }
    Containers::Pointer<AbstractMaterialData> material = importer->material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);

    PhongMaterialData* phongMaterial = static_cast<PhongMaterialData*>(material.get());
    CORRADE_VERIFY(phongMaterial);
    CORRADE_COMPARE(phongMaterial->flags(), PhongMaterialData::Flag::AmbientTexture|PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture);
    CORRADE_COMPARE(phongMaterial->ambientTexture(), 0);
    CORRADE_COMPARE(phongMaterial->diffuseTexture(), 1);
    CORRADE_COMPARE(phongMaterial->specularTexture(), 2);

    /* Alpha not supported by OBJ, should be set to 1 */
    CORRADE_COMPARE(phongMaterial->ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 1.0f}));
    CORRADE_COMPARE(phongMaterial->diffuseColor(), (Color4{0.08f, 0.16f, 0.24f, 1.0f}));
    CORRADE_COMPARE(phongMaterial->specularColor(), (Color4{0.15f, 0.1f, 0.05f, 1.0f}));
}

void AssimpImporterTest::materialStlWhiteAmbientPatch() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "quad.stl")));

    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Pointer<AbstractMaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material(0);
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    {
        /* aiGetVersion*() returns 401 for assimp 5, FFS, so we have to check
           differently. See CMakeLists.txt for details. */
        CORRADE_EXPECT_FAIL_IF(version < 401 || ASSIMP_IS_VERSION_5,
            "Assimp < 4.1 and >= 5.0 behaves properly regarding STL material ambient");
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::material(): white ambient detected, forcing back to black\n");
    }

    auto& phongMaterial = static_cast<PhongMaterialData&>(*material);
    CORRADE_COMPARE(phongMaterial.flags(), PhongMaterialData::Flags{});
    /* WHY SO COMPLICATED, COME ON */
    if(version < 401 || ASSIMP_IS_VERSION_5)
        CORRADE_COMPARE(phongMaterial.ambientColor(), Color3{0.05f});
    else
        CORRADE_COMPARE(phongMaterial.ambientColor(), 0x000000_srgbf);

    /* ASS IMP WHAT?! WHY 3.2 is different from 3.0 and 4.0?! */
    if(version == 302) {
        CORRADE_COMPARE(phongMaterial.specularColor(), Color3{0.6f});
        CORRADE_COMPARE(phongMaterial.diffuseColor(), Color3{0.6f});
    } else {
        CORRADE_COMPARE(phongMaterial.specularColor(), 0xffffff_srgbf);
        CORRADE_COMPARE(phongMaterial.diffuseColor(), 0xffffff_srgbf);
    }
    /* This value is not supplied by Assimp for STL models, so we set it to 0 */
    CORRADE_COMPARE(phongMaterial.shininess(), 0.0f);
}

void AssimpImporterTest::materialWhiteAmbientTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "texture-ambient.obj")));

    /* ASS IMP reports TWO materials for an OBJ. The parser code is so lazy
       that it just has the first material totally empty. Wonderful. Lost one
       hour on this and my hair is even greyer now. */
    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 1);
    } {
        CORRADE_COMPARE(importer->materialCount(), 2);
    }

    Containers::Pointer<AbstractMaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material(1);
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);
    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(static_cast<PhongMaterialData&>(*material).flags(), PhongMaterialData::Flag::AmbientTexture);
    /* It shouldn't be complaining about white ambient in this case */
    CORRADE_COMPARE(out.str(), "");
}

void AssimpImporterTest::materialMultipleTextures() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "multiple-textures.obj")));

    /* See materialWhiteAmbientTexture() for a rant. */
    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 3);
    } {
        CORRADE_COMPARE(importer->materialCount(), 3 + 1);
    }

    /* Seven textures, but using just four distinct images */
    CORRADE_COMPARE(importer->textureCount(), 7);
    CORRADE_COMPARE(importer->image2DCount(), 4);

    /* Check that texture ID assignment is correct */
    {
        Containers::Pointer<AbstractMaterialData> material = importer->material(importer->materialForName("ambient_diffuse"));
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), MaterialType::Phong);

        auto& phong = static_cast<PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flag::AmbientTexture|PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_COMPARE(phong.ambientTexture(), 0); /* r.png */
        CORRADE_COMPARE(phong.diffuseTexture(), 1); /* g.png */
    } {
        Containers::Pointer<AbstractMaterialData> material = importer->material(importer->materialForName("diffuse_specular"));
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), MaterialType::Phong);

        auto& phong = static_cast<PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 2); /* b.png */
        CORRADE_COMPARE(phong.specularTexture(), 3); /* y.png */
    } {
        Containers::Pointer<AbstractMaterialData> material = importer->material(importer->materialForName("all"));
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), MaterialType::Phong);

        auto& phong = static_cast<PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flag::AmbientTexture|PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture);
        CORRADE_COMPARE(phong.ambientTexture(), 4); /* y.png */
        CORRADE_COMPARE(phong.diffuseTexture(), 5); /* r.png */
        CORRADE_COMPARE(phong.specularTexture(), 6); /* g.png */
    }

    /* Check that image ID assignment is correct */
    {
        Containers::Optional<TextureData> texture = importer->texture(0);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 0); /* r.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(1);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 1); /* g.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(2);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 2); /* b.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(3);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 3); /* y.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(4);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 3); /* y.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(5);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 0); /* r.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(6);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 1); /* g.png */
    }

    /* Check that correct images are imported */
    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0xff0000_rgb); /* r.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x00ff00_rgb); /* g.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb); /* b.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(3);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0xffff00_rgb); /* y.png */
    }
}

void AssimpImporterTest::materialTextureCoordinateSetsDefault() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-coordinate-sets.dae")));

    CORRADE_EXPECT_FAIL("Material is loaded since coordinate sets are not imported");
    CORRADE_VERIFY(!importer->material(0));

    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::material(): multiple texture coordinate sets are not allowed by default, enable allowMaterialTextureCoordinateSets to import them\n");
}

void AssimpImporterTest::materialTextureCoordinateSets() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("allowMaterialTextureCoordinateSets", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-coordinate-sets.dae")));
    auto abstractMat = importer->material(0);
    auto& mat = static_cast<PhongMaterialData&>(*abstractMat);

    CORRADE_EXPECT_FAIL("Assimp ignores ambient texture and does not import coordinate sets");
    CORRADE_COMPARE(mat.flags(), PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::NormalTexture|PhongMaterialData::Flag::SpecularTexture|PhongMaterialData::Flag::TextureCoordinateSets);
    CORRADE_COMPARE(mat.diffuseCoordinateSet(), 2);
    CORRADE_COMPARE(mat.specularCoordinateSet(), 3);
    CORRADE_COMPARE(mat.normalCoordinateSet(), 2);
}

void AssimpImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Tangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Tangent),
        Containers::arrayView<Vector3>({
            {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Bitangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Bitangent),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.9f}
        }), TestSuite::Compare::Container);

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    {
        CORRADE_EXPECT_FAIL_IF(version < 302,
            "Assimp < 3.2 loads incorrect alpha value for the last color");
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color),
        Containers::arrayView<Vector4>({
            {1.0f, 0.25f, 0.24f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.1f, 0.2f, 0.3f, 1.0f}
        }), TestSuite::Compare::Container);
    }

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::pointMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "points.obj")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 0}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {0.5f, 2.0f, 3.0f}, {2.0f, 3.0f, 5.0f}, {0.0f, 1.5f, 1.0f}
        }), TestSuite::Compare::Container);

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::lineMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "line.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Lines);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::meshMultiplePrimitives() {
    /* Possibly broken in other versions too (4.1 and 5 works, 3.2 doesn't) */
    if(aiGetVersionMajor()*100 + aiGetVersionMinor() <= 302)
        CORRADE_SKIP("Assimp 3.2 doesn't recognize primitives used in the test COLLADA file.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "mesh-multiple-primitives.dae")));

    /* Four meshes, but one has three primitives and one two. Distinguishing
       using the primitive type, hopefully that's enough. */
    CORRADE_COMPARE(importer->meshCount(), 5);
    {
        auto mesh0 = importer->mesh(0);
        CORRADE_VERIFY(mesh0);
        CORRADE_COMPARE(mesh0->primitive(), MeshPrimitive::Triangles);
        auto mesh1 = importer->mesh(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
    } {
        auto mesh2 = importer->mesh(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Lines);
        auto mesh3 = importer->mesh(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::Triangles);
        auto mesh4 = importer->mesh(4);
        CORRADE_VERIFY(mesh4);
        CORRADE_COMPARE(mesh4->primitive(), MeshPrimitive::Triangles);
    }

    /* Five objects, but two refer a three-primitive mesh and one refers a
       two-primitive one */
    CORRADE_COMPARE(importer->object3DCount(), 9);
    {
        CORRADE_COMPARE(importer->object3DName(0), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DName(1), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DName(2), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DForName("Using_the_second_mesh__should_have_4_children"), 0);
        auto object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2, 7}));

        auto child1 = importer->object3D(1);
        CORRADE_VERIFY(child1);
        CORRADE_COMPARE(child1->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child1->instance(), 3);
        CORRADE_COMPARE(child1->children(), {});
        CORRADE_COMPARE(child1->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child1->translation(), Vector3{});
        CORRADE_COMPARE(child1->rotation(), Quaternion{});
        CORRADE_COMPARE(child1->scaling(), Vector3{1.0f});

        auto child2 = importer->object3D(2);
        CORRADE_VERIFY(child2);
        CORRADE_COMPARE(child2->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child2->instance(), 4);
        CORRADE_COMPARE(child2->children(), {});
        CORRADE_COMPARE(child2->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child2->translation(), Vector3{});
        CORRADE_COMPARE(child2->rotation(), Quaternion{});
        CORRADE_COMPARE(child2->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(3), "Just_a_non-mesh_node");
        CORRADE_COMPARE(importer->object3DForName("Just_a_non-mesh_node"), 3);
        auto object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->children(), {});
    } {
        CORRADE_COMPARE(importer->object3DName(4), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DName(5), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DName(6), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DForName("Using_the_second_mesh_again__again_2_children"), 4);
        auto object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{5, 6}));

        auto child5 = importer->object3D(5);
        CORRADE_VERIFY(child5);
        CORRADE_COMPARE(child5->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child5->instance(), 3);
        CORRADE_COMPARE(child5->children(), {});
        CORRADE_COMPARE(child5->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child5->translation(), Vector3{});
        CORRADE_COMPARE(child5->rotation(), Quaternion{});
        CORRADE_COMPARE(child5->scaling(), Vector3{1.0f});

        auto child6 = importer->object3D(6);
        CORRADE_VERIFY(child6);
        CORRADE_COMPARE(child6->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child6->instance(), 4);
        CORRADE_COMPARE(child6->children(), {});
        CORRADE_COMPARE(child6->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child6->translation(), Vector3{});
        CORRADE_COMPARE(child6->rotation(), Quaternion{});
        CORRADE_COMPARE(child6->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(7), "Using_the_fourth_mesh__1_child");
        CORRADE_COMPARE(importer->object3DName(8), "Using_the_fourth_mesh__1_child");
        CORRADE_COMPARE(importer->object3DForName("Using_the_fourth_mesh__1_child"), 7);
        auto object = importer->object3D(7);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{8}));

        auto child8 = importer->object3D(8);
        CORRADE_VERIFY(child8);
        CORRADE_COMPARE(child8->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child8->instance(), 1);
        CORRADE_COMPARE(child8->children(), {});
        CORRADE_COMPARE(child8->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child8->translation(), Vector3{});
        CORRADE_COMPARE(child8->rotation(), Quaternion{});
        CORRADE_COMPARE(child8->scaling(), Vector3{1.0f});
    }
}

void AssimpImporterTest::emptyCollada() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* Instead of giving out an empty file, assimp fails on opening, but only
       for COLLADA, not for e.g. glTF. I have a different opinion about the
       behavior, but whatever. It's also INTERESTING that supplying an empty
       DAE through file callbacks results in a completely different message --
       see fileCallbackEmptyFile(). */
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "empty.dae")));
}

void AssimpImporterTest::emptyGltf() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    if(version < 401)
        CORRADE_SKIP("glTF 2 is supported since Assimp 4.1.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "empty.gltf")));
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 0);
    CORRADE_COMPARE(importer->object3DCount(), 0);

    /* No crazy meshes created for an empty glTF file, unlike with COLLADA
       files that have no meshes */
    CORRADE_COMPARE(importer->meshCount(), 0);
}

void AssimpImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    Containers::Pointer<ObjectData3D> parent = importer->object3D(0);
    CORRADE_COMPARE(parent->children(), {1});
    CORRADE_COMPARE(parent->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(parent->transformation(), Matrix4::scaling({1.0f, 2.0f, 3.0f}));

    Containers::Pointer<ObjectData3D> childObject = importer->object3D(1);
    CORRADE_COMPARE(childObject->transformation(), (Matrix4{
        {0.813798f, 0.469846f, -0.34202f, 0.0f},
        {-0.44097f, 0.882564f, 0.163176f, 0.0f},
        {0.378522f, 0.0180283f, 0.925417f, 0.0f},
        {1.0f, 2.0f, 3.0f, 1.0f}}));

    CORRADE_COMPARE(importer->object3DForName("Parent"), 0);
    CORRADE_COMPARE(importer->object3DForName("Child"), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Parent");
    CORRADE_COMPARE(importer->object3DName(1), "Child");

    CORRADE_COMPARE(importer->object3DForName("Ghost"), -1);
}

void AssimpImporterTest::sceneCollapsedNode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* This collapses all nodes into one. Neither OptimizeGraph nor
       OptimizeMeshes does that, but this one does it. Um. */
    importer->configuration().group("postprocess")->setValue("PreTransformVertices", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1); /* Just the root node */

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    /* Assimp makes some bogus mesh for this one */
    Containers::Pointer<ObjectData3D> collapsedNode = importer->object3D(0);
    CORRADE_COMPARE(collapsedNode->children(), {});
    CORRADE_COMPARE(collapsedNode->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(collapsedNode->transformation(), Matrix4{});

    /* Name of the scene is used for the root object */
    {
        const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
        /** @todo Possibly works with other versions (definitely not 3.0) */
        CORRADE_EXPECT_FAIL_IF(version <= 302,
            "Assimp 3.2 and below doesn't use name of the root node for collapsed nodes.");
        CORRADE_COMPARE(importer->object3DForName("Scene"), 0);
        CORRADE_COMPARE(importer->object3DName(0), "Scene");
    }
}

void AssimpImporterTest::upDirectionPatching() {
    auto&& data = UpDirectionPatchingData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Set only if not false to test correctness of the default as well */
    if(data.importColladaIgnoreUpDirection)
        importer->configuration().setValue("ImportColladaIgnoreUpDirection", true);
    importer->configuration().setValue("ImportColladaIgnoreUpDirection",
        data.importColladaIgnoreUpDirection);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    /* First object is directly in the root, second object is a child of the
       first. */
    Matrix4 object0Transformation, object1Transformation;
    {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(0);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{1});
        object0Transformation = meshObject->transformation();
    } {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(1);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{});
        object1Transformation = meshObject->transformation();
    }

    /* The first mesh should have always the same final positions independently
       of how file's Y/Z-up or PreTransformVertices is set */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        /* Transform the positions with object transform */
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        MeshTools::transformPointsInPlace(object0Transformation,
            mesh->mutableAttribute<Vector3>(MeshAttribute::Position));

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}
            }), TestSuite::Compare::Container);

    /* The second mesh is a child of the first, scaled 2x in addition. Verify
       the initial Z-up pretransformation is not applied redundantly to it. */
    } {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        /* Transform the positions with object transform and its parent as
           well */
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        MeshTools::transformPointsInPlace(
            object0Transformation*object1Transformation,
            mesh->mutableAttribute<Vector3>(MeshAttribute::Position));

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-2.0f, 2.0f, -2.0f}, {-2.0f, 2.0f, 2.0f}
            }), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::upDirectionPatchingPreTransformVertices() {
    auto&& data = UpDirectionPatchingData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Set only if not false to test correctness of the default as well */
    if(data.importColladaIgnoreUpDirection)
        importer->configuration().setValue("ImportColladaIgnoreUpDirection", true);
    importer->configuration().group("postprocess")->setValue("PreTransformVertices", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    /* There's only one object, directly in the root, with no transformation */
    {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(0);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{});
        CORRADE_COMPARE(meshObject->transformation(), Matrix4{});
    }

    /* There's just one mesh, with all vertices combined and already
       transformed. */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f},
                {-2.0f, 2.0f, -2.0f}, {-2.0f, 2.0f, 2.0f}
            }), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::imageEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    if(version <= 302)
        CORRADE_SKIP("Assimp < 3.2 can't load embedded textures in blend files, Assimp 3.2 can't detect blend file format when opening a memory location.");

    /* Open as data, so we verify opening embedded images from data does not
       cause any problems even when no file callbacks are set */
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "embedded-texture.blend"))));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternal() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternalNotFound() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly fails on more versions (definitely w/ 3.0 and 3.2) */
    if(version <= 302)
        CORRADE_SKIP("Assimp <= 3.2 would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-not-found.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    /* The (failed) importer should get cached even in case of failure, so
       the message should get printed just once */
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /not-found.png\n");
}

void AssimpImporterTest::imageExternalNoPathNoCallback() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void AssimpImporterTest::imagePathMtlSpaceAtTheEnd() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-filename-trailing-space.obj")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageMipLevels() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");
    if(_manager.loadState("DdsImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("DdsImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-mips.obj")));
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(1), 1);

    /* Verify that loading a different image will properly switch to another
       importer instance */
    Containers::Optional<ImageData2D> image00 = importer->image2D(0);
    Containers::Optional<ImageData2D> image01 = importer->image2D(0, 1);
    Containers::Optional<ImageData2D> image1 = importer->image2D(1);

    CORRADE_VERIFY(image00);
    CORRADE_COMPARE(image00->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image00->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image00->data(), Containers::arrayView<char>({
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77'
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image01);
    CORRADE_COMPARE(image01->size(), Vector2i{1});
    CORRADE_COMPARE(image01->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image01->data(), Containers::arrayView<char>({
        '\xd4', '\xd5', '\x96'
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image1);
    CORRADE_COMPARE(image1->size(), Vector2i{1});
    CORRADE_COMPARE(image1->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image1->data(), Containers::arrayView<char>({
        '\xb3', '\x69', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::texture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->textureCount(), 3);
    Containers::Optional<TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->image(), 0);

    Containers::Optional<TextureData> texture1 = importer->texture(1);
    CORRADE_VERIFY(texture1);
    CORRADE_COMPARE(texture1->type(), TextureData::Type::Texture2D);
    {
        /* I assume this "don't care for remaining stuff" part is responsible:
           https://github.com/assimp/assimp/blob/0c3933ca7c460644d346d94ecbb1b118f598ced4/code/Collada/ColladaParser.cpp#L1977-L1978 */
        CORRADE_EXPECT_FAIL("Assimp ignores sampler properties (in COLLADA files, at least).");
        CORRADE_COMPARE(texture1->wrapping(),
            Array3D<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
        CORRADE_COMPARE(texture1->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture1->magnificationFilter(), SamplerFilter::Nearest);
    } {
        /* It gives out the default always */
        CORRADE_COMPARE(texture->wrapping(),
            Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    }
    CORRADE_COMPARE(texture1->image(), 1);

    /* Normal texture, reusing the diffuse image (so the same index) */
    Containers::Optional<TextureData> texture2 = importer->texture(2);
    CORRADE_VERIFY(texture2);
    CORRADE_COMPARE(texture2->type(), TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture2->image(), 0);

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::openState() {
    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->openState(sc);
    CORRADE_VERIFY(importer->isOpened());

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    Containers::Pointer<ObjectData3D> parent = importer->object3D(0);
    CORRADE_COMPARE(parent->children(), {1});
    CORRADE_COMPARE(parent->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(parent->transformation(), Matrix4::scaling({1.0f, 2.0f, 3.0f}));

    Containers::Pointer<ObjectData3D> childObject = importer->object3D(1);
    CORRADE_COMPARE(childObject->transformation(), (Matrix4{
        {0.813798f, 0.469846f, -0.34202f, 0.0f},
        {-0.44097f, 0.882564f, 0.163176f, 0.0f},
        {0.378522f, 0.0180283f, 0.925417f, 0.0f},
        {1.0f, 2.0f, 3.0f, 1.0f}}));

    CORRADE_COMPARE(importer->object3DForName("Parent"), 0);
    CORRADE_COMPARE(importer->object3DForName("Child"), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Parent");
    CORRADE_COMPARE(importer->object3DName(1), "Child");
}

void AssimpImporterTest::openStateTexture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openState(sc, ASSIMPIMPORTER_TEST_DIR));
    CORRADE_COMPARE(importer->importerState(), sc);

    CORRADE_COMPARE(importer->textureCount(), 3);
    Containers::Optional<TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::configurePostprocessFlipUVs() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().group("postprocess")->setValue("FlipUVs", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 1);

    /* The same as in mesh() but with reversed Y */
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 0.0f}, {0.75f, 0.5f}, {0.5f, 0.1f}
        }), TestSuite::Compare::Container);
}

void AssimpImporterTest::fileCallback() {
    /* This should verify also formats with external data (such as glTF),
       because Assimp is using the same callbacks for all data loading */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/mesh.dae"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae"));
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/mesh.dae"));
    CORRADE_COMPARE(importer->meshCount(), 1);

    /* Same as in mesh(), testing just the basics, no need to repeat everything
       here */
    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
}

void AssimpImporterTest::fileCallbackNotFound() {
    /* This should verify also formats with external data (such as glTF),
       because Assimp is using the same callbacks for all data loading */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.dae"));

    /* Assimp 5.0 changed the error string. aiGetVersion*() returns 401 for
       assimp 5, FFS, so we have to check differently. See CMakeLists.txt for
       details. */
    if(ASSIMP_IS_VERSION_5)
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file 'some-file.dae'.\n");
    else
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file some-file.dae.\n");
}

void AssimpImporterTest::fileCallbackEmptyFile() {
    /* This verifies that we don't do anything silly (like division by zero) in
       IoStream::Read(). Works only with *.dae files, for *.obj Assimp bails
       out with `OBJ-file is too small.` without even calling Read(). */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{Containers::InPlaceInit};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.dae"));
    /* INTERESTINGLY ENOUGH, a different message is printed when opening a DAE
       file directly w/o callbacks -- see emptyCollada() above. */
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: File is too small\n");
}

void AssimpImporterTest::fileCallbackReset() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    /* Verify that nothing crashes/leaks here ... and also doesn't double free */
    importer->setFileCallback(nullptr);
    CORRADE_VERIFY(true);
}

void AssimpImporterTest::fileCallbackImage() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/texture.dae"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"));
    files["not/a/path/diffuse_texture.png"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "diffuse_texture.png"));
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/texture.dae"));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 1));
}

void AssimpImporterTest::fileCallbackImageNotFound() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file diffuse_texture.png\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
