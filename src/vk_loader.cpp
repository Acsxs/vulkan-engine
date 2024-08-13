#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

//std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* engine, std::filesystem::path filePath) {
//    std::cout << "Loading GLTF: " << filePath << std::endl;
//
//    fastgltf::GltfDataBuffer data;
//    data.loadFromFile(filePath);
//
//    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
//
//    fastgltf::Asset gltf;
//    fastgltf::Parser parser{};
//
//    auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
//    if (load) {
//        gltf = std::move(load.get());
//    }
//    else {
//        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
//        return {};
//    }
//
//    std::vector<std::shared_ptr<MeshAsset>> meshes;
//
//    // use the same vectors for all meshes so that the memory doesnt reallocate as
//    // often
//    std::vector<uint32_t> indices;
//    std::vector<Vertex> vertices;
//    for (fastgltf::Mesh& mesh : gltf.meshes) {
//        MeshAsset newMesh;
//
//        newMesh.name = mesh.name;
//
//        indices.clear();
//        vertices.clear();
//
//        for (auto&& primitive : mesh.primitives) {
//            GeoSurface newSurface;
//            newSurface.startIndex = (uint32_t)indices.size();
//            newSurface.count = (uint32_t)gltf.accessors[primitive.indicesAccessor.value()].count;
//
//            size_t initialVertex = vertices.size();
//
//            // load indexes
//            {
//                fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
//                indices.reserve(indices.size() + indexAccessor.count);
//
//                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](std::uint32_t idx) { indices.push_back(idx + initialVertex); });
//            }
//
//            // load vertex positions
//            {
//                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
//                vertices.resize(vertices.size() + posAccessor.count);
//
//                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
//                    [&](glm::vec3 v, size_t index) {
//                        Vertex newVertex;
//                        newVertex.position = v;
//                        newVertex.normal = { 1, 0, 0 };
//                        newVertex.color = glm::vec4{ 1.f };
//                        newVertex.uv_x = 0;
//                        newVertex.uv_y = 0;
//                        vertices[initialVertex + index] = newVertex;
//                    });
//            }
//
//            // load vertex normals
//            auto normals = primitive.findAttribute("NORMAL");
//            if (normals != primitive.attributes.end()) {
//
//                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
//                    [&](glm::vec3 v, size_t index) {
//                        vertices[initialVertex + index].normal = v;
//                    });
//            }
//
//            // load UVs
//            auto uv = primitive.findAttribute("TEXCOORD_0");
//            if (uv != primitive.attributes.end()) {
//
//                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
//                    [&](glm::vec2 v, size_t index) {
//                        vertices[initialVertex + index].uv_x = v.x;
//                        vertices[initialVertex + index].uv_y = v.y;
//                    });
//            }
//
//            // load vertex colors
//            auto colors = primitive.findAttribute("COLOR_0");
//            if (colors != primitive.attributes.end()) {
//
//                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
//                    [&](glm::vec4 v, size_t index) {
//                        vertices[initialVertex + index].color = v;
//                    });
//            }
//            newMesh.surfaces.push_back(newSurface);
//        }
//
//        // display the vertex normals
//        if (bOverrideColors) {
//            for (Vertex& vtx : vertices) {
//                vtx.color = glm::vec4(vtx.normal, 1.f);
//            }
//        }
//        newMesh.meshBuffers = engine->uploadMesh(indices, vertices);
//
//        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
//    }
//
//    return meshes;
//}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        }
        else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        }
        else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } 
    };

    file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);

    for (fastgltf::Sampler& sampler : gltf.samplers) {

        VkSamplerCreateInfo samplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        samplerInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &samplerInfo, nullptr, &newSampler);

        file.samplers.push_back(newSampler);


    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    for (fastgltf::Image& image : gltf.images) {
        images.push_back(engine->_errorCheckerboardImage);
    }

    // create buffer to hold the material data
    file.materialDataBuffer = engine->createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;

    GLTFMetallicRoughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallicRoughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallicRoughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metallicRoughnessFactors.x = mat.pbrData.metallicFactor;
        constants.metallicRoughnessFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallicRoughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallicRoughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }
        // build material
        newMat->data = engine->metalRoughMaterial.writeMaterial(engine->_device, passType, materialResources, file.descriptorPool);

        data_index++;
    }

}

static VkFilter extractFilter(fastgltf::Filter filter)
{
    switch (filter) {
        // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

        // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

static VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}
