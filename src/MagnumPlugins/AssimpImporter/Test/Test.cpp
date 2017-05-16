/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>

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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>

#include <MagnumPlugins/AssimpImporter/AssimpImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

using namespace Magnum::Math::Literals;

struct AssimpImporterTest: public TestSuite::Tester {
    explicit AssimpImporterTest();

    void open();

    void camera();

    void object();
    void objectCamera();
    void objectLight();
    void objectMesh();
    void objectTransformation();

    void light();

    void mesh();

    void materialColors();
    void materialTextured();

    void texture();

    void image();
};

AssimpImporterTest::AssimpImporterTest() {
    addTests({&AssimpImporterTest::open,

              &AssimpImporterTest::camera,

              &AssimpImporterTest::object,
              &AssimpImporterTest::objectCamera,
              &AssimpImporterTest::objectLight,
              &AssimpImporterTest::objectMesh,
              &AssimpImporterTest::objectTransformation,

              &AssimpImporterTest::light,

              &AssimpImporterTest::mesh,

              &AssimpImporterTest::materialColors,
              &AssimpImporterTest::materialTextured,

              &AssimpImporterTest::texture,

              &AssimpImporterTest::image});
}

void AssimpImporterTest::open() {
    AssimpImporter importer;

    // TODO
}

void AssimpImporterTest::camera() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera.ogex")));
    CORRADE_COMPARE(importer.cameraCount(), 2);

    /* Everything specified */
    {
        std::optional<Trade::CameraData> camera = importer.camera(0);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 0.97_radf);
        CORRADE_COMPARE(camera->near(), 1.5f);
        CORRADE_COMPARE(camera->far(), 150.0f);

    /* Nothing specified (defaults) */
    } {
        std::optional<Trade::CameraData> camera = importer.camera(1);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), Rad{35.0_degf});
        CORRADE_COMPARE(camera->near(), 0.01f);
        CORRADE_COMPARE(camera->far(), 100.0f);
    }
}

void AssimpImporterTest::object() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object.ogex")));
    CORRADE_COMPARE(importer.sceneCount(), 1);
    CORRADE_COMPARE(importer.object3DCount(), 5);

    std::optional<Trade::SceneData> scene = importer.scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{0, 3}));

    std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(importer.object3DName(0), "MyNode");
    CORRADE_COMPARE(importer.object3DForName("MyNode"), 0);
    CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2}));

    std::unique_ptr<Trade::ObjectData3D> cameraObject = importer.object3D(1);
    CORRADE_VERIFY(cameraObject);
    CORRADE_COMPARE(cameraObject->instanceType(), Trade::ObjectInstanceType3D::Camera);

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer.object3D(2);
    CORRADE_VERIFY(meshObject);
    CORRADE_COMPARE(importer.object3DName(2), "MyGeometryNode");
    CORRADE_COMPARE(importer.object3DForName("MyGeometryNode"), 2);
    CORRADE_COMPARE(meshObject->instanceType(), Trade::ObjectInstanceType3D::Mesh);
    CORRADE_VERIFY(meshObject->children().empty());

    std::unique_ptr<Trade::ObjectData3D> boneObject = importer.object3D(3);
    CORRADE_VERIFY(boneObject);
    CORRADE_COMPARE(boneObject->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(boneObject->children(), (std::vector<UnsignedInt>{4}));

    std::unique_ptr<Trade::ObjectData3D> lightObject = importer.object3D(4);
    CORRADE_VERIFY(lightObject);
    CORRADE_COMPARE(lightObject->instanceType(), Trade::ObjectInstanceType3D::Light);
    CORRADE_VERIFY(lightObject->children().empty());
}

void AssimpImporterTest::objectCamera() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-camera.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 2);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Camera);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::object3D(): null camera reference\n");
}

void AssimpImporterTest::objectLight() {
    CORRADE_SKIP("assimp segfaults this test because of assimp/assimp#1262");

    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-light.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 2);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Light);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::object3D(): null light reference\n");
}

void AssimpImporterTest::objectMesh() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-geometry.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 4);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.instance(), 1);
        CORRADE_COMPARE(meshObject.material(), 2);
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(3));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::object3D(): null geometry reference\n");
}

void AssimpImporterTest::objectTransformation() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 3);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {3.0f,  0.0f, 0.0f, 0.0f},
            {0.0f, -2.0f, 0.0f, 0.0f},
            {0.0f,  0.0f, 0.5f, 0.0f},
            {7.5f, -1.5f, 1.0f, 1.0f}
        }));
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_VERIFY(!importer.object3D(2));
    CORRADE_COMPARE(out.str(),
        "Trade::AssimpImporter::object3D(): invalid transformation\n"
        "Trade::AssimpImporter::object3D(): unsupported object-only transformation\n");
}

void AssimpImporterTest::light() {
    CORRADE_SKIP("assimp segfaults this test because of assimp/assimp#1262");

    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "light.ogex")));
    CORRADE_COMPARE(importer.lightCount(), 3);

    /* Infinite light, everything specified */
    {
        std::optional<Trade::LightData> light = importer.light(0);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Infinite);
        CORRADE_COMPARE(light->color(), (Color3{0.7f, 1.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);

    /* Point light, default color */
    } {
        std::optional<Trade::LightData> light = importer.light(1);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 0.5f);

    /* Spot light, default intensity */
    } {
        std::optional<Trade::LightData> light = importer.light(2);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.1f, 0.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    }
}

void AssimpImporterTest::mesh() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    CORRADE_COMPARE(1, importer.mesh3DCount());
    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleStrip);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 2);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.5f}, {0.5f, 1.0f}, {1.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2D(1), (std::vector<Vector2>{
        {0.5f, 1.0f}, {1.0f, 0.5f}, {0.5f, 0.5f}
    }));
}

void AssimpImporterTest::materialColors() {
    AssimpImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);

    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);
    CORRADE_COMPARE(importer.materialName(1), "colors");
    CORRADE_COMPARE(importer.materialForName("colors"), 1);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(!phong.flags());
    CORRADE_COMPARE(phong.diffuseColor(), (Vector3{0.0f, 0.8f, 0.5f}));
    CORRADE_COMPARE(phong.specularColor(), (Vector3{0.5f, 0.2f, 1.0f}));
    CORRADE_COMPARE(phong.shininess(), 80.0f);
}

void AssimpImporterTest::materialTextured() {
    AssimpImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);
    CORRADE_COMPARE(importer.textureCount(), 4);

    {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(2);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(2), "diffuse_texture");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == Trade::PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
    } {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(3);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(3), "both_textures");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == (Trade::PhongMaterialData::Flag::DiffuseTexture|Trade::PhongMaterialData::Flag::SpecularTexture));
        CORRADE_COMPARE(phong.diffuseTexture(), 2);
        CORRADE_COMPARE(phong.specularTexture(), 3);
    }
}

void AssimpImporterTest::texture() {
    CORRADE_SKIP("assimp segfaults this test because of assimp/assimp#1262");
    AssimpImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 2);

    std::optional<Trade::TextureData> texture = importer.texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->minificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->wrapping(), Sampler::Wrapping::ClampToEdge);
    CORRADE_COMPARE(texture->image(), 1);
}

void AssimpImporterTest::image() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    AssimpImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    std::optional<Trade::ImageData2D> image = importer.image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
