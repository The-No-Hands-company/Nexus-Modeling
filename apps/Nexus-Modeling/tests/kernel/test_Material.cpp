#include <gtest/gtest.h>

#include <nexus/material/Material.h>

using namespace nexus::material;

TEST(PBRMaterial, NoTexturesByDefault)
{
    PBRMaterial mat;
    EXPECT_FALSE(mat.hasTexture(TextureType::Albedo));
    EXPECT_FALSE(mat.hasTexture(TextureType::Normal));
    EXPECT_FALSE(mat.hasTexture(TextureType::Roughness));
    EXPECT_FALSE(mat.hasTexture(TextureType::Metallic));
}

TEST(PBRMaterial, HasTextureReturnsTrueWhenPresent)
{
    PBRMaterial mat;
    mat.textures.push_back({{"albedo.png"}, TextureType::Albedo});
    EXPECT_TRUE(mat.hasTexture(TextureType::Albedo));
    EXPECT_FALSE(mat.hasTexture(TextureType::Normal));
}

TEST(PBRMaterial, HasTextureMultipleTypes)
{
    PBRMaterial mat;
    mat.textures.push_back({{"albedo.png"}, TextureType::Albedo});
    mat.textures.push_back({{"normal.png"}, TextureType::Normal});
    EXPECT_TRUE(mat.hasTexture(TextureType::Albedo));
    EXPECT_TRUE(mat.hasTexture(TextureType::Normal));
    EXPECT_FALSE(mat.hasTexture(TextureType::Roughness));
}

TEST(MaterialLibrary, AddAndRetrieveMaterial)
{
    MaterialLibrary lib;
    EXPECT_EQ(lib.materialCount(), 0u);

    PBRMaterial mat;
    mat.name = "test";
    mat.albedo = {1.0f, 0.0f, 0.0f, 1.0f};
    mat.roughness = 0.3f;

    uint32_t idx = lib.addMaterial(mat);
    EXPECT_EQ(idx, 0u);
    EXPECT_EQ(lib.materialCount(), 1u);

    const auto& retrieved = lib.material(idx);
    EXPECT_EQ(retrieved.name, "test");
    EXPECT_FLOAT_EQ(retrieved.roughness, 0.3f);
}

TEST(MaterialLibrary, MultipleMaterialsSequentialIndices)
{
    MaterialLibrary lib;
    PBRMaterial a, b, c;

    uint32_t ia = lib.addMaterial(a);
    uint32_t ib = lib.addMaterial(b);
    uint32_t ic = lib.addMaterial(c);

    EXPECT_EQ(ia, 0u);
    EXPECT_EQ(ib, 1u);
    EXPECT_EQ(ic, 2u);
    EXPECT_EQ(lib.materialCount(), 3u);
}

TEST(MaterialLibrary, SlotsEmptyByDefault)
{
    MaterialLibrary lib;
    EXPECT_TRUE(lib.slots().empty());
}

TEST(MaterialLibrary, AddSlotAppends)
{
    MaterialLibrary lib;
    MaterialSlot slot;
    slot.materialIndex = 0;
    slot.targetMeshName = "mesh_a";

    lib.addSlot(slot);
    EXPECT_EQ(lib.slots().size(), 1u);
    EXPECT_EQ(lib.slots()[0].targetMeshName, "mesh_a");
}

TEST(MaterialLibrary, MutableMaterialAccessWorks)
{
    MaterialLibrary lib;
    PBRMaterial initial;
    initial.roughness = 0.5f;
    uint32_t idx = lib.addMaterial(initial);

    auto& mat = lib.material(idx);
    mat.roughness = 0.8f;

    EXPECT_FLOAT_EQ(lib.material(idx).roughness, 0.8f);
}

TEST(PBRMaterial, DefaultValuesAreSane)
{
    PBRMaterial mat;
    EXPECT_FLOAT_EQ(mat.albedo.x, 0.8f);
    EXPECT_FLOAT_EQ(mat.albedo.y, 0.8f);
    EXPECT_FLOAT_EQ(mat.albedo.z, 0.8f);
    EXPECT_FLOAT_EQ(mat.albedo.w, 1.0f);
    EXPECT_FLOAT_EQ(mat.roughness, 0.5f);
    EXPECT_FLOAT_EQ(mat.metallic, 0.0f);
    EXPECT_FLOAT_EQ(mat.ao, 1.0f);
    EXPECT_FLOAT_EQ(mat.opacity, 1.0f);
}
