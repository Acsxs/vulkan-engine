﻿
#define STB_IMAGE_IMPLEMENTATION 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"


#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#include "tiny_gltf.h"

#include <iostream>
#include "vk_descriptors.h"

#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/normal.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <tuple>




VkFilter extractFilter(fastgltf::Filter filter)
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

VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
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

std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                // local files.

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                    // specify LoadExternalBuffers, meaning all buffers
                    // are already loaded into a vector.
                    [](auto& arg) {}, [&](fastgltf::sources::Vector& vector) {
                        unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                        if (data) {
                            VkExtent3D imagesize;
                            imagesize.width = width;
                            imagesize.height = height;
                            imagesize.depth = 1;

                            newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_USAGE_SAMPLED_BIT,false);

                            stbi_image_free(data);
                        }
                    }
                },
            buffer.data);
            },
        },
        image.data);

    // if all of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    }
    else {
        return newImage;
    }
}

// Make this a compute shader
std::vector<Vertex> recompileMesh(std::vector<uint32_t> indices, std::vector<Vertex> vertices) {
    std::vector<Vertex> newVertices = vertices;
    for (int i = 0; i < indices.size(); i += 3) {
        Vertex v1 = newVertices[indices[i]];
        Vertex v2 = newVertices[indices[i + 1]];
        Vertex v3 = newVertices[indices[i + 2]];

        glm::vec3 dPos1 = v2.position - v1.position;
        glm::vec3 dPos2 = v3.position - v1.position;

        glm::vec2 dUV1 = glm::vec2(v2.uv_x - v1.uv_x, v2.uv_y - v1.uv_y);
        glm::vec2 dUV2 = glm::vec2(v3.uv_x - v1.uv_x, v3.uv_y - v1.uv_y);

        float r = 1.0f / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);
        glm::vec4 tangent = glm::vec4((dPos1 * dUV2.y - dPos2 * dUV1.y) * r, 0.f);
        glm::vec4 biTangent = glm::vec4((dPos2 * dUV1.x - dPos1 * dUV2.x) * r, 0.f);

        newVertices[indices[i]].tangent += tangent;
        newVertices[indices[i]].biTangent += biTangent;
        newVertices[indices[i + 1]].tangent += tangent;
        newVertices[indices[i + 1]].biTangent += biTangent;
        newVertices[indices[i + 2]].tangent += tangent;
        newVertices[indices[i + 2]].biTangent += biTangent;
    }
    /*for (int i = 0; i < vertices.size(); i++) {
        Vertex vertex = newVertices[i];
        newVertices[i].tangent = glm::normalize(vertex.tangent);
        newVertices[i].biTangent = glm::normalize(vertex.biTangent);
    }*/
    return newVertices;
}


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* engine, std::filesystem::path filePath) {
    std::cout << "Loading GLTF: " << filePath << std::endl;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
    if (load) {
        gltf = std::move(load.get());
    }
    else {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newMesh;

        newMesh.name = mesh.name;

        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[primitive.indicesAccessor.value()].count;

            size_t initialVertex = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](std::uint32_t idx) { indices.push_back(idx + initialVertex); });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newVertex;
                        newVertex.position = v;
                        newVertex.normal = { 1, 0, 0 };
                        newVertex.color = glm::vec4{ 1.f };
                        newVertex.uv_x = 0;
                        newVertex.uv_y = 0;
                        vertices[initialVertex + index] = newVertex;
                    });
            }

            // load vertex normals
            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVertex + index].normal = v;
                    });
            }

            // load UVs
            auto uv = primitive.findAttribute("TEXCOORD_0");
            if (uv != primitive.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initialVertex + index].uv_x = v.x;
                        vertices[initialVertex + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = primitive.findAttribute("COLOR_0");
            if (colors != primitive.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initialVertex + index].color = v;
                    });
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }
        newMesh.meshBuffers = engine->uploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}

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
        std::optional<AllocatedImage> img = loadImage(engine, gltf, image);

        if (img.has_value()) {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
        }
        else {
            // we failed to load, so lets give the slot a default white texture to not
            // completely break loading
            images.push_back(engine->_errorCheckerboardImage);
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    // create buffer to hold the material data
    file.materialDataBuffer = engine->createBuffer(sizeof(GLTFSpecularRoughness::MaterialConstants) * gltf.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;

    GLTFSpecularRoughness::MaterialConstants* sceneMaterialConstants = (GLTFSpecularRoughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFSpecularRoughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.specularRoughnessFactors.x = mat.pbrData.metallicFactor;
        constants.specularRoughnessFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        GLTFSpecularRoughness::MaterialResources materialResources;
        // default the material textures
        materialResources.albedo = engine->_whiteImage;
        materialResources.albedoSampler = engine->_defaultSamplerLinear;
        materialResources.specularRoughnessImage = engine->_greyImage;
        materialResources.specularRoughnessSampler = engine->_defaultSamplerLinear;
        materialResources.normal = engine->_blackImage;
        materialResources.normalSampler = engine->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFSpecularRoughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.albedo = images[img];
            materialResources.albedoSampler = file.samplers[sampler];
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();

            materialResources.specularRoughnessImage = images[img];
            materialResources.specularRoughnessSampler = file.samplers[sampler];
        }
        if (mat.normalTexture.has_value()) {
            size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            materialResources.normal = images[img];
        }
        newMat->data = engine->specularRoughnessMaterial.writeMaterial(engine->_device, passType, materialResources, file.descriptorPool);

        data_index++;
    }

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        newvtx.tangent = glm::vec4{ 0.f };
                        newvtx.biTangent = glm::vec4{ 0.f };
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else {
                newSurface.material = materials[0];
            }

            newmesh->surfaces.push_back(newSurface);
        }
        vertices = recompileMesh(indices, vertices);

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
    }


    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
        }
        else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{ [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](fastgltf::Node::TRS transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4{ 1.f });
        }
    }

    return scene;
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll() {
    VkDevice dv = creator->_device;

    descriptorPool.destroyPools(dv);
    creator->destroyBuffer(materialDataBuffer);

    for (auto& [k, v] : meshes) {

        creator->destroyBuffer(v->meshBuffers.indexBuffer);
        creator->destroyBuffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) {

        if (v.image == creator->_errorCheckerboardImage.image) {
            //dont destroy the default images
            continue;
        }
        creator->destroyImage(v);
    }

    for (auto& sampler : samplers) {
        vkDestroySampler(dv, sampler, nullptr);
    }
}

//bool loadModel(tinygltf::Model* model, std::string filePath) {
//    fmt::println ("Loading GLTF: {}", filePath);
//    
//    tinygltf::TinyGLTF tinyGLTF;
//
//    std::string err;
//    std::string warn;
//
//    bool res = tinyGLTF.LoadASCIIFromFile(model, &err, &warn, filePath);
//    if (!warn.empty()) {
//        std::cout << "WARN: " << warn << std::endl;
//    }
//
//    if (!err.empty()) {
//        std::cout << "ERR: " << err << std::endl;
//    }
//    if (!res)
//        std::cout << "Failed to load glTF: " << filePath << std::endl;
//    else
//        std::cout << "Loaded glTF: " << filePath << std::endl;
//    return res;
//}
