#include <vk_mesh.hpp>

#include <tiny_obj_loader.h>

#include <cstddef>
#include <cstdint>
#include <iostream>

VertexInputDescription Vertex::getVertexInputDescription() {
  VertexInputDescription description;

  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(Vertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(mainBinding);

  VkVertexInputAttributeDescription positionAttribute = {};
  positionAttribute.binding = 0;
  positionAttribute.location = 0;
  positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  positionAttribute.offset = offsetof(Vertex, position);

  description.attributes.push_back(positionAttribute);

  VkVertexInputAttributeDescription normalAttribute = {};
  normalAttribute.binding = 0;
  normalAttribute.location = 1;
  normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  normalAttribute.offset = offsetof(Vertex, normal);

  description.attributes.push_back(normalAttribute);

  VkVertexInputAttributeDescription colorAttribute = {};
  colorAttribute.binding = 0;
  colorAttribute.location = 2;
  colorAttribute.format = VK_FORMAT_R32G32_SFLOAT;
  colorAttribute.offset = offsetof(Vertex, color);

  description.attributes.push_back(colorAttribute);

  return description;
}

bool Mesh::loadFromObj(char const *filePath) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warning, error;

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, filePath,
                   nullptr);

  if (!warning.empty()) {
    std::cout << "tinyobj warning: " << warning << std::endl;
  }

  if (!error.empty()) {
    std::cout << "tinyobj error: " << error << std::endl;
    return false;
  }

  for (std::size_t s = 0; s < shapes.size(); ++s) {
    tinyobj::shape_t const &shape = shapes[s];

    std::size_t faceIndOffset = 0;

    for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      unsigned char const vertexCount = shape.mesh.num_face_vertices[f];

      for (std::size_t v = 0; v < vertexCount; ++v) {
        tinyobj::index_t const idx = shape.mesh.indices[faceIndOffset + v];

        tinyobj::real_t const posX = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t const posY = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t const posZ = attrib.vertices[3 * idx.vertex_index + 2];
        tinyobj::real_t const normalX =
            attrib.normals[3 * idx.normal_index + 0];
        tinyobj::real_t const normalY =
            attrib.normals[3 * idx.normal_index + 1];
        tinyobj::real_t const normalZ =
            attrib.normals[3 * idx.normal_index + 2];

        _vertices.emplace_back(glm::vec3{posX, posY, posZ},
                               glm::vec3{normalX, normalY, normalZ},
                               glm::vec3{normalX, normalY, normalZ});
      }

      faceIndOffset += vertexCount;
    }
  }
  return true;
}
