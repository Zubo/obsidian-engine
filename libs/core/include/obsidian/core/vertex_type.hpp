#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

namespace obsidian::core {

using MeshIndexType = std::uint32_t;

template <bool HasNormal, bool HasColor, bool HasUV> struct VertexAttr {
  static constexpr bool hasNormal = HasNormal;
  static constexpr bool hasColor = HasColor;
  static constexpr bool hasUV = HasUV;
  static constexpr bool hasTangent = HasNormal && HasUV;
};

template <bool HasNormal, bool HasColor, bool HasUV> struct VertexType {};

template <>
struct VertexType<false, false, false>
    : public VertexAttr<false, false, false> {
  struct Vertex {
    glm::vec3 pos;
  };
};

template <>
struct VertexType<true, false, false> : public VertexAttr<true, false, false> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
  };
};

template <>
struct VertexType<false, false, true> : public VertexAttr<false, false, true> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
  };
};

template <>
struct VertexType<false, true, false> : public VertexAttr<false, true, false> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
  };
};

template <>
struct VertexType<true, true, false> : public VertexAttr<true, true, false> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
  };
};

template <>
struct VertexType<true, false, true> : public VertexAttr<true, false, true> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
  };
};

template <>
struct VertexType<false, true, true> : public VertexAttr<false, true, true> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
  };
};

template <>
struct VertexType<true, true, true> : public VertexAttr<true, true, true> {
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    glm::vec3 tangent;
  };
};

} /*namespace obsidian::core*/
