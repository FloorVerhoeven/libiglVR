// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#include "MeshGL.h"
#include "bind_vertex_attrib_array.h"
#include "create_shader_program.h"
#include "destroy_shader_program.h"
#include <iostream>

IGL_INLINE void igl::opengl::MeshGL::init_buffers()
{
  // Mesh: Vertex Array Object & Buffer objects
  glGenVertexArrays(1, &vao_mesh);
  glBindVertexArray(vao_mesh);
  glGenBuffers(1, &vbo_V);
  glGenBuffers(1, &vbo_V_normals);
  glGenBuffers(1, &vbo_V_ambient);
  glGenBuffers(1, &vbo_V_diffuse);
  glGenBuffers(1, &vbo_V_specular);
  glGenBuffers(1, &vbo_V_uv);
  glGenBuffers(1, &vbo_F);
  glGenTextures(1, &vbo_tex);

  // Line overlay
  glGenVertexArrays(1, &vao_overlay_lines);
  glBindVertexArray(vao_overlay_lines);
  glGenBuffers(1, &vbo_lines_F);
  glGenBuffers(1, &vbo_lines_V);
  glGenBuffers(1, &vbo_lines_V_colors);

  // Point overlay
  glGenVertexArrays(1, &vao_overlay_points);
  glBindVertexArray(vao_overlay_points);
  glGenBuffers(1, &vbo_points_F);
  glGenBuffers(1, &vbo_points_V);
  glGenBuffers(1, &vbo_points_V_colors);

  // Stroke overlay
  glGenVertexArrays(1, &vao_stroke_points);
  glBindVertexArray(vao_stroke_points);
  glGenBuffers(1, &vbo_stroke_points_F);
  glGenBuffers(1, &vbo_stroke_points_V);

  // Laser overlay
  glGenVertexArrays(1, &vao_laser_points);
  glBindVertexArray(vao_laser_points);
  glGenBuffers(1, &vbo_laser_points_F);
  glGenBuffers(1, &vbo_laser_points_V);

  // Hand marker overlay
  glGenVertexArrays(1, &vao_hand_point);
  glBindVertexArray(vao_hand_point);
  glGenBuffers(1, &vbo_hand_point_F);
  glGenBuffers(1, &vbo_hand_point_V);
  glGenBuffers(1, &vbo_hand_point_V_colors);

  // Avatar overlay
  glGenVertexArrays(1, &vao_avatar);
  glBindVertexArray(vao_avatar);
  glGenBuffers(1, &vbo_avatar_F);
  glGenBuffers(1, &vbo_avatar_V);


  dirty = MeshGL::DIRTY_ALL;
}

IGL_INLINE void igl::opengl::MeshGL::free_buffers()
{
  if (is_initialized)
  {
    glDeleteVertexArrays(1, &vao_mesh);
    glDeleteVertexArrays(1, &vao_overlay_lines);
    glDeleteVertexArrays(1, &vao_overlay_points);
	glDeleteVertexArrays(1, &vao_stroke_points);
	glDeleteVertexArrays(1, &vao_laser_points);
	glDeleteVertexArrays(1, &vao_hand_point);
	glDeleteVertexArrays(1, &vao_avatar);

    glDeleteBuffers(1, &vbo_V);
    glDeleteBuffers(1, &vbo_V_normals);
    glDeleteBuffers(1, &vbo_V_ambient);
    glDeleteBuffers(1, &vbo_V_diffuse);
    glDeleteBuffers(1, &vbo_V_specular);
    glDeleteBuffers(1, &vbo_V_uv);
    glDeleteBuffers(1, &vbo_F);
    glDeleteBuffers(1, &vbo_lines_F);
    glDeleteBuffers(1, &vbo_lines_V);
    glDeleteBuffers(1, &vbo_lines_V_colors);
    glDeleteBuffers(1, &vbo_points_F);
    glDeleteBuffers(1, &vbo_points_V);
    glDeleteBuffers(1, &vbo_points_V_colors);
	glDeleteBuffers(1, &vbo_stroke_points_V);
	glDeleteBuffers(1, &vbo_stroke_points_F);
	glDeleteBuffers(1, &vbo_laser_points_V);
	glDeleteBuffers(1, &vbo_laser_points_F);
	glDeleteBuffers(1, &vbo_hand_point_F);
	glDeleteBuffers(1, &vbo_hand_point_V);
	glDeleteBuffers(1, &vbo_hand_point_V_colors);
	glDeleteBuffers(1, &vbo_avatar_F);
	glDeleteBuffers(1, &vbo_avatar_V);


    glDeleteTextures(1, &vbo_tex);
  }
}

IGL_INLINE void igl::opengl::MeshGL::bind_mesh()
{
  glBindVertexArray(vao_mesh);
  glUseProgram(shader_mesh);
  bind_vertex_attrib_array(shader_mesh,"position", vbo_V, V_vbo, dirty & MeshGL::DIRTY_POSITION);
  bind_vertex_attrib_array(shader_mesh,"normal", vbo_V_normals, V_normals_vbo, dirty & MeshGL::DIRTY_NORMAL);
  bind_vertex_attrib_array(shader_mesh,"Ka", vbo_V_ambient, V_ambient_vbo, dirty & MeshGL::DIRTY_AMBIENT);
  bind_vertex_attrib_array(shader_mesh,"Kd", vbo_V_diffuse, V_diffuse_vbo, dirty & MeshGL::DIRTY_DIFFUSE);
  bind_vertex_attrib_array(shader_mesh,"Ks", vbo_V_specular, V_specular_vbo, dirty & MeshGL::DIRTY_SPECULAR);
  bind_vertex_attrib_array(shader_mesh,"texcoord", vbo_V_uv, V_uv_vbo, dirty & MeshGL::DIRTY_UV);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_F);
  if (dirty & MeshGL::DIRTY_FACE)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*F_vbo.size(), F_vbo.data(), GL_DYNAMIC_DRAW);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, vbo_tex);
  if (dirty & MeshGL::DIRTY_TEXTURE)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_u, tex_v, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data());
  }
  glUniform1i(glGetUniformLocation(shader_mesh,"tex"), 0);
  dirty &= ~MeshGL::DIRTY_MESH;
}

IGL_INLINE void igl::opengl::MeshGL::bind_overlay_lines()
{
  bool is_dirty = dirty & MeshGL::DIRTY_OVERLAY_LINES;

  glBindVertexArray(vao_overlay_lines);
  glUseProgram(shader_overlay_lines);
 bind_vertex_attrib_array(shader_overlay_lines,"position", vbo_lines_V, lines_V_vbo, is_dirty);
 bind_vertex_attrib_array(shader_overlay_lines,"color", vbo_lines_V_colors, lines_V_colors_vbo, is_dirty);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_lines_F);
  if (is_dirty)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*lines_F_vbo.size(), lines_F_vbo.data(), GL_DYNAMIC_DRAW);

  dirty &= ~MeshGL::DIRTY_OVERLAY_LINES;
}

IGL_INLINE void igl::opengl::MeshGL::bind_overlay_points()
{
  bool is_dirty = dirty & MeshGL::DIRTY_OVERLAY_POINTS;

  glBindVertexArray(vao_overlay_points);
  glUseProgram(shader_overlay_points);

 bind_vertex_attrib_array(shader_overlay_points,"position", vbo_points_V, points_V_vbo, is_dirty);
 bind_vertex_attrib_array(shader_overlay_points,"color", vbo_points_V_colors, points_V_colors_vbo, is_dirty);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_points_F);
  if (is_dirty)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*points_F_vbo.size(), points_F_vbo.data(), GL_DYNAMIC_DRAW);

  dirty &= ~MeshGL::DIRTY_OVERLAY_POINTS;
}

IGL_INLINE void igl::opengl::MeshGL::bind_stroke() {
	bool is_dirty = dirty & MeshGL::DIRTY_STROKE;

	glBindVertexArray(vao_stroke_points);
	glUseProgram(shader_stroke_points);
	bind_vertex_attrib_array(shader_stroke_points, "position", vbo_stroke_points_V, stroke_points_V_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_stroke_points_F);
	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*stroke_points_F_vbo.size(), stroke_points_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_STROKE;
}

IGL_INLINE void igl::opengl::MeshGL::bind_laser() {
	bool is_dirty = dirty & MeshGL::DIRTY_LASER;

	glBindVertexArray(vao_laser_points);
	glUseProgram(shader_laser_points);

	bind_vertex_attrib_array(shader_laser_points,"position", vbo_laser_points_V, laser_points_V_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_laser_points_F);
	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*laser_points_F_vbo.size(), laser_points_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_LASER;
}

IGL_INLINE void igl::opengl::MeshGL::bind_hand_point()
{
	bool is_dirty = dirty & MeshGL::DIRTY_HAND_POINT;

	glBindVertexArray(vao_hand_point);  
	glUseProgram(shader_hand_point);
	bind_vertex_attrib_array(shader_hand_point, "position", vbo_hand_point_V, hand_point_V_vbo, is_dirty);
	bind_vertex_attrib_array(shader_hand_point,"color", vbo_hand_point_V_colors, hand_point_V_colors_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_hand_point_F);

	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*hand_point_F_vbo.size(), hand_point_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_HAND_POINT;
}

IGL_INLINE void igl::opengl::MeshGL::bind_avatar() {
	bool is_dirty = dirty & MeshGL::DIRTY_AVATAR;
	glBindVertexArray(vao_avatar);
	glUseProgram(shader_avatar);
	bind_vertex_attrib_array(shader_avatar, "position", vbo_avatar_V, avatar_V_vbo, is_dirty);
	bind_vertex_attrib_array(shader_avatar, "normal", vbo_avatar_V_normals, avatar_V_normals_vbo, is_dirty);
	bind_vertex_attrib_array(shader_avatar, "tangent", vbo_avatar_V_tangents, avatar_V_tangents_vbo, is_dirty);
	bind_vertex_attrib_array(shader_avatar, "texCoord", vbo_avatar_V_tex, avatar_V_tex_vbo, is_dirty);
	bind_vertex_attrib_array(shader_avatar, "poseIndices", vbo_avatar_V_poseIndices, avatar_V_poseIndices_vbo, is_dirty);
	bind_vertex_attrib_array(shader_avatar, "poseWeights", vbo_avatar_V_poseWeights, avatar_V_poseWeights_vbo, is_dirty);
	//bind_vertex_attrib_array(shader_avatar, "colors", vbo_avatar_V_colors, avatar_V_colors_vbo, is_dirty);


	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_avatar_F);

	if (is_dirty) {
		std::cout << "hit" << std::endl;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*avatar_F_vbo.size(), avatar_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_AVATAR;
}

IGL_INLINE void igl::opengl::MeshGL::draw_mesh(bool solid)
{
  glPolygonMode(GL_FRONT_AND_BACK, solid ? GL_FILL : GL_LINE);

  /* Avoid Z-buffer fighting between filled triangles & wireframe lines */
  if (solid)
  {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);
  }
  glDrawElements(GL_TRIANGLES, 3*F_vbo.rows(), GL_UNSIGNED_INT, 0);

  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

IGL_INLINE void igl::opengl::MeshGL::draw_overlay_lines()
{
  glDrawElements(GL_LINES, lines_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_overlay_points()
{
  glDrawElements(GL_POINTS, points_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_stroke()
{
	glDrawElements(GL_LINE_STRIP, stroke_points_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_laser()
{
	glDrawElements(GL_LINE_STRIP, laser_points_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_hand_point()
{
	glDrawElements(GL_POINTS, hand_point_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::init()
{
  if(is_initialized)
  {
    return;
  }
  is_initialized = true;
  std::string mesh_vertex_shader_string =
R"(#version 150
  uniform mat4 model;
  //uniform mat4 model_trans;
  uniform mat4 view;
  uniform mat4 proj;
  in vec3 position;
  in vec3 normal;
  out vec3 position_eye;
  out vec3 normal_eye;
  in vec4 Ka;
  in vec4 Kd;
  in vec4 Ks;
  in vec2 texcoord;
  out vec2 texcoordi;
  out vec4 Kai;
  out vec4 Kdi;
  out vec4 Ksi;

  void main()
  {
    position_eye = vec3 (view * model * vec4 (position, 1.0)); //TODO: document that model_trans was added here
    normal_eye = vec3 (view * model * vec4 (normal, 0.0)); //TODO: document that model_trans was added here
    normal_eye = normalize(normal_eye);
    gl_Position = proj * vec4 (position_eye, 1.0); //proj * view * model * vec4(position, 1.0);
    Kai = Ka;
    Kdi = Kd;
    Ksi = Ks;
    texcoordi = texcoord;
  }
)";

  std::string mesh_fragment_shader_string = 
R"(#version 150
  uniform mat4 model;
  uniform mat4 view;
  uniform mat4 proj;
  uniform vec4 fixed_color;
  in vec3 position_eye;
  in vec3 normal_eye;
  uniform vec3 light_position_world;
  vec3 Ls = vec3 (1, 1, 1);
  vec3 Ld = vec3 (1, 1, 1);
  vec3 La = vec3 (1, 1, 1);
  in vec4 Ksi;
  in vec4 Kdi;
  in vec4 Kai;
  in vec2 texcoordi;
  uniform sampler2D tex;
  uniform float specular_exponent;
  uniform float lighting_factor;
  uniform float texture_factor;
  out vec4 outColor;
  void main()
  {
    vec3 Ia = La * vec3(Kai);    // ambient intensity

    vec3 light_position_eye = vec3 (view * vec4 (light_position_world, 1.0));
    vec3 vector_to_light_eye = light_position_eye - position_eye;
    vec3 direction_to_light_eye = normalize (vector_to_light_eye);
    float dot_prod = dot (direction_to_light_eye, normal_eye);
    float clamped_dot_prod = max (dot_prod, 0.0);
    vec3 Id = Ld * vec3(Kdi) * clamped_dot_prod;    // Diffuse intensity

    vec3 reflection_eye = reflect (-direction_to_light_eye, normal_eye);
    vec3 surface_to_viewer_eye = normalize (-position_eye);
    float dot_prod_specular = dot (reflection_eye, surface_to_viewer_eye);
    dot_prod_specular = float(abs(dot_prod)==dot_prod) * max (dot_prod_specular, 0.0);
    float specular_factor = pow (dot_prod_specular, specular_exponent);
    vec3 Is = Ls * vec3(Ksi) * specular_factor;    // specular intensity
    vec4 color = vec4(lighting_factor * (Is + Id) + Ia + (1.0-lighting_factor) * vec3(Kdi),(Kai.a+Ksi.a+Kdi.a)/3);
    outColor = mix(vec4(1,1,1,1), texture(tex, texcoordi), texture_factor) * color;
    if (fixed_color != vec4(0.0)) outColor = fixed_color;
  }
  )";

  std::string overlay_vertex_shader_string =
R"(#version 150
  uniform mat4 model;
 // uniform mat4 model_trans;
  uniform mat4 view;
  uniform mat4 proj;
  in vec3 position;
  in vec3 color;
  out vec3 color_frag;

  void main()
  {
    gl_Position = proj * view * model * vec4 (position, 1.0); //TODO: document that model_trans was added here
    color_frag = color;
  }
)";

  std::string stroke_vertex_shader_string =
	  R"(#version 150
	  uniform mat4 model;
	  uniform mat4 view;
	  uniform mat4 proj;
	  in vec3 position;

	  void main()
	  {
	    gl_Position = proj * view * model * vec4 (position, 1.0);
	  }
)";

  std::string laser_vertex_shader_string =
	  R"(#version 150
	  uniform mat4 model;
	  uniform mat4 view;
	  uniform mat4 proj;
	  in vec3 position;

	  void main()
	  {
	    gl_Position = proj * view * model * vec4 (position, 1.0);
	  }
)";

  std::string overlay_fragment_shader_string =
R"(#version 150
  in vec3 color_frag;
  out vec4 outColor;
  void main()
  {
    outColor = vec4(color_frag, 1.0);
  }
)";

  std::string overlay_point_fragment_shader_string =
R"(#version 150
  in vec3 color_frag;
  out vec4 outColor;
  void main()
  {
    if (length(gl_PointCoord - vec2(0.5)) > 0.5)
      discard;
    outColor = vec4(color_frag, 1.0);
  }
)"; 

//Note: removed part that discards points that are on a coordinate that ain't big enough (otherwise it won't draw anything)
  /*std::string overlay_point_fragment_shader_string =
	  R"(#version 150
	  in vec3 color_frag;
	  out vec4 outColor;
	  void main()
	  {
	    outColor = vec4(color_frag, 1.0);
	  }
)";*/

  std::string overlay_stroke_fragment_shader_string =
	  R"(#version 150
	  out vec4 outColor;
	  void main()
	  {
	    outColor = vec4(1.0, 0.0, 0.0, 1.0);
	  }
)";

  std::string overlay_laser_fragment_shader_string =
	  R"(#version 150
	  out vec4 outColor;
	  void main()
	  {
	    outColor = vec4(1.0, 0.0, 0.0, 1.0);
	  }
)";

  std::string hand_fragment_shader_string =
	  R"(#version 150
	  in vec3 color_frag;
	  out vec4 outColor;
	  void main()
	  {
	    outColor = vec4(color_frag, 1.0);
	  }
)";
 /* std::string hand_fragment_shader_string =
	  R"(#version 150
  in vec3 color_frag;
  out vec4 outColor;
  void main()
  {
    if (length(gl_PointCoord - vec2(0.5)) > 0.5)
      discard;
    outColor = vec4(color_frag, 1.0);
  }
)";*/

  std::string avatar_vertex_shader_string =
	  R"(#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord;
layout (location = 4) in vec4 poseIndices;
layout (location = 5) in vec4 poseWeights;
layout (location = 6) in vec4 color;
out vec3 vertexWorldPos;
out vec3 vertexViewDir;
out vec3 vertexObjPos;
out vec3 vertexNormal;
out vec3 vertexTangent;
out vec3 vertexBitangent;
out vec2 vertexUV;
out vec4 vertexColor;
uniform vec3 viewPos;
uniform mat4 world;
uniform mat4 viewProj;
uniform mat4 meshPose[64];
void main() {
    vec4 vertexPose;
    vertexPose = meshPose[int(poseIndices[0])] * vec4(position, 1.0) * poseWeights[0];
    vertexPose += meshPose[int(poseIndices[1])] * vec4(position, 1.0) * poseWeights[1];
    vertexPose += meshPose[int(poseIndices[2])] * vec4(position, 1.0) * poseWeights[2];
    vertexPose += meshPose[int(poseIndices[3])] * vec4(position, 1.0) * poseWeights[3];
    
    vec4 normalPose;
    normalPose = meshPose[int(poseIndices[0])] * vec4(normal, 0.0) * poseWeights[0];
    normalPose += meshPose[int(poseIndices[1])] * vec4(normal, 0.0) * poseWeights[1];
    normalPose += meshPose[int(poseIndices[2])] * vec4(normal, 0.0) * poseWeights[2];
    normalPose += meshPose[int(poseIndices[3])] * vec4(normal, 0.0) * poseWeights[3];
    normalPose = normalize(normalPose);

	vec4 tangentPose;
    tangentPose = meshPose[int(poseIndices[0])] * vec4(tangent.xyz, 0.0) * poseWeights[0];
    tangentPose += meshPose[int(poseIndices[1])] * vec4(tangent.xyz, 0.0) * poseWeights[1];
    tangentPose += meshPose[int(poseIndices[2])] * vec4(tangent.xyz, 0.0) * poseWeights[2];
    tangentPose += meshPose[int(poseIndices[3])] * vec4(tangent.xyz, 0.0) * poseWeights[3];
	tangentPose = normalize(tangentPose);

	vertexWorldPos = vec3(world * vertexPose);
	gl_Position = viewProj * vec4(vertexWorldPos, 1.0);
	vertexViewDir = normalize(viewPos - vertexWorldPos.xyz);
    vertexObjPos = position.xyz;
	vertexNormal = (world * normalPose).xyz;
	vertexTangent = (world * tangentPose).xyz;
	vertexBitangent = normalize(cross(vertexNormal, vertexTangent) * tangent.w);
	vertexUV = texCoord;
	vertexColor = color.rgba;
})";

  std::string avatar_fragment_shader_string = 
 R"( #version 330 core

#define SAMPLE_MODE_COLOR 0
#define SAMPLE_MODE_TEXTURE 1
#define SAMPLE_MODE_TEXTURE_SINGLE_CHANNEL 2
#define SAMPLE_MODE_PARALLAX 3
#define SAMPLE_MODE_RSRM 4

#define MASK_TYPE_NONE 0
#define MASK_TYPE_POSITIONAL 1
#define MASK_TYPE_REFLECTION 2
#define MASK_TYPE_FRESNEL 3
#define MASK_TYPE_PULSE 4

#define BLEND_MODE_ADD 0
#define BLEND_MODE_MULTIPLY 1

#define MAX_LAYER_COUNT 8

	  in vec3 vertexWorldPos;
  in vec3 vertexViewDir;
  in vec3 vertexObjPos;
  in vec3 vertexNormal;
  in vec3 vertexTangent;
  in vec3 vertexBitangent;
  in vec2 vertexUV;
  out vec4 fragmentColor;

  uniform vec4 baseColor;
  uniform int baseMaskType;
  uniform vec4 baseMaskParameters;
  uniform vec4 baseMaskAxis;
  uniform sampler2D alphaMask;
  uniform vec4 alphaMaskScaleOffset;
  uniform sampler2D normalMap;
  uniform vec4 normalMapScaleOffset;
  uniform sampler2D parallaxMap;
  uniform vec4 parallaxMapScaleOffset;
  uniform sampler2D roughnessMap;
  uniform vec4 roughnessMapScaleOffset;

  uniform mat4 projectorInv;

  uniform bool useAlpha;
  uniform bool useNormalMap;
  uniform bool useRoughnessMap;
  uniform bool useProjector;
  uniform float elapsedSeconds;

  uniform int layerCount;

  uniform int layerSamplerModes[MAX_LAYER_COUNT];
  uniform int layerBlendModes[MAX_LAYER_COUNT];
  uniform int layerMaskTypes[MAX_LAYER_COUNT];
  uniform vec4 layerColors[MAX_LAYER_COUNT];
  uniform sampler2D layerSurfaces[MAX_LAYER_COUNT];
  uniform vec4 layerSurfaceScaleOffsets[MAX_LAYER_COUNT];
  uniform vec4 layerSampleParameters[MAX_LAYER_COUNT];
  uniform vec4 layerMaskParameters[MAX_LAYER_COUNT];
  uniform vec4 layerMaskAxes[MAX_LAYER_COUNT];

  vec3 ComputeNormal(mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal, float surfaceStrength)
  {
	  if (useNormalMap)
	  {
		  vec3 surface = mix(vec3(0.0, 0.0, 1.0), surfaceNormal, surfaceStrength);
		  return normalize(surface * tangentTransform);
	  }
	  else
	  {
		  return worldNormal;
	  }
  }

  vec3 ComputeColor(int sampleMode, vec2 uv, vec4 color, sampler2D surface, vec4 surfaceScaleOffset, vec4 sampleParameters, mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal)
  {
	  if (sampleMode == SAMPLE_MODE_TEXTURE)
	  {
		  vec2 panning = elapsedSeconds * sampleParameters.xy;
		  return texture(surface, (uv + panning) * surfaceScaleOffset.xy + surfaceScaleOffset.zw).rgb * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_TEXTURE_SINGLE_CHANNEL)
	  {
		  vec4 channelMask = sampleParameters;
		  vec4 channels = texture(surface, uv * surfaceScaleOffset.xy + surfaceScaleOffset.zw);
		  return dot(channelMask, channels) * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_PARALLAX)
	  {
		  float parallaxMinHeight = sampleParameters.x;
		  float parallaxMaxHeight = sampleParameters.y;
		  float parallaxValue = texture(parallaxMap, uv * parallaxMapScaleOffset.xy + parallaxMapScaleOffset.zw).r;
		  float scaledHeight = mix(parallaxMinHeight, parallaxMaxHeight, parallaxValue);
		  vec2 parallaxUV = (vertexViewDir * tangentTransform).xy * scaledHeight;

		  return texture(surface, (uv + parallaxUV) * surfaceScaleOffset.xy + surfaceScaleOffset.zw).rgb * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_RSRM)
	  {
		  float roughnessMin = sampleParameters.x;
		  float roughnessMax = sampleParameters.y;

		  float scaledRoughness = roughnessMin;
		  if (useRoughnessMap)
		  {
			  float roughnessValue = texture(roughnessMap, uv * roughnessMapScaleOffset.xy + roughnessMapScaleOffset.zw).r;
			  scaledRoughness = mix(roughnessMin, roughnessMax, roughnessValue);
		  }

		  float normalMapStrength = sampleParameters.z;
		  vec3 viewReflect = reflect(-vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength));
		  float viewAngle = viewReflect.y * 0.5 + 0.5;
		  return texture(surface, vec2(scaledRoughness, viewAngle)).rgb * color.rgb;
	  }
	  return color.rgb;
  }

  float ComputeMask(int maskType, vec4 maskParameters, vec4 maskAxis, mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal)
  {
	  if (maskType == MASK_TYPE_POSITIONAL) {
		  float centerDistance = maskParameters.x;
		  float fadeAbove = maskParameters.y;
		  float fadeBelow = maskParameters.z;
		  float d = dot(vertexObjPos, maskAxis.xyz);
		  if (d > centerDistance) {
			  return clamp(1.0 - (d - centerDistance) / fadeAbove, 0.0, 1.0);
		  }
		  else {
			  return clamp(1.0 - (centerDistance - d) / fadeBelow, 0.0, 1.0);
		  }
	  }
	  else if (maskType == MASK_TYPE_REFLECTION) {
		  float fadeStart = maskParameters.x;
		  float fadeEnd = maskParameters.y;
		  float normalMapStrength = maskParameters.z;
		  vec3 viewReflect = reflect(-vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength));
		  float d = dot(viewReflect, maskAxis.xyz);
		  return clamp(1.0 - (d - fadeStart) / (fadeEnd - fadeStart), 0.0, 1.0);
	  }
	  else if (maskType == MASK_TYPE_FRESNEL) {
		  float power = maskParameters.x;
		  float fadeStart = maskParameters.y;
		  float fadeEnd = maskParameters.z;
		  float normalMapStrength = maskParameters.w;
		  float d = 1.0 - max(0.0, dot(vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength)));
		  float p = pow(d, power);
		  return clamp(mix(fadeStart, fadeEnd, p), 0.0, 1.0);
	  }
	  else if (maskType == MASK_TYPE_PULSE) {
		  float distance = maskParameters.x;
		  float speed = maskParameters.y;
		  float power = maskParameters.z;
		  float d = dot(vertexObjPos, maskAxis.xyz);
		  float theta = 6.2831 * fract((d - elapsedSeconds * speed) / distance);
		  return clamp(pow((sin(theta) * 0.5 + 0.5), power), 0.0, 1.0);
	  }
	  else {
		  return 1.0;
	  }
	  return 1.0;
  }

  vec3 ComputeBlend(int blendMode, vec3 dst, vec3 src, float mask)
  {
	  if (blendMode == BLEND_MODE_MULTIPLY)
	  {
		  return dst * (src * mask);
	  }
	  else {
		  return dst + src * mask;
	  }
  }

  void main() {
	  vec3 worldNormal = normalize(vertexNormal);
	  mat3 tangentTransform = mat3(vertexTangent, vertexBitangent, worldNormal);

	  vec2 uv = vertexUV;
	  if (useProjector)
	  {
		  vec4 projectorPos = projectorInv * vec4(vertexWorldPos, 1.0);
		  if (abs(projectorPos.x) > 1.0 || abs(projectorPos.y) > 1.0 || abs(projectorPos.z) > 1.0)
		  {
			  discard;
			  return;
		  }
		  uv = projectorPos.xy * 0.5 + 0.5;
	  }

	  vec3 surfaceNormal = vec3(0.0, 0.0, 1.0);
	  if (useNormalMap)
	  {
		  surfaceNormal.xy = texture2D(normalMap, uv * normalMapScaleOffset.xy + normalMapScaleOffset.zw).xy * 2.0 - 1.0;
		  surfaceNormal.z = sqrt(1.0 - dot(surfaceNormal.xy, surfaceNormal.xy));
	  }

	  vec4 color = baseColor;
	  for (int i = 0; i < layerCount; ++i)
	  {
		  vec3 layerColor = ComputeColor(layerSamplerModes[i], uv, layerColors[i], layerSurfaces[i], layerSurfaceScaleOffsets[i], layerSampleParameters[i], tangentTransform, worldNormal, surfaceNormal);
		  float layerMask = ComputeMask(layerMaskTypes[i], layerMaskParameters[i], layerMaskAxes[i], tangentTransform, worldNormal, surfaceNormal);
		  color.rgb = ComputeBlend(layerBlendModes[i], color.rgb, layerColor, layerMask);
	  }

	  if (useAlpha)
	  {
		  color.a *= texture(alphaMask, uv * alphaMaskScaleOffset.xy + alphaMaskScaleOffset.zw).r;
	  }
	  color.a *= ComputeMask(baseMaskType, baseMaskParameters, baseMaskAxis, tangentTransform, worldNormal, surfaceNormal);
	  fragmentColor = color;
  })";

  init_buffers();
  create_shader_program(
    mesh_vertex_shader_string,
    mesh_fragment_shader_string,
    {},
    shader_mesh);
  create_shader_program(
    overlay_vertex_shader_string,
    overlay_fragment_shader_string,
    {},
    shader_overlay_lines);
  create_shader_program(
    overlay_vertex_shader_string,
    overlay_point_fragment_shader_string,
    {},
    shader_overlay_points);
  create_shader_program(
	  stroke_vertex_shader_string,
	  overlay_stroke_fragment_shader_string,
	  {},
	  shader_stroke_points);
  create_shader_program(
	  laser_vertex_shader_string,
	  overlay_laser_fragment_shader_string,
	  {},
	  shader_laser_points);
  create_shader_program(
	  overlay_vertex_shader_string,
	  hand_fragment_shader_string,
	  {},
	  shader_hand_point);
  create_shader_program(
	  avatar_vertex_shader_string,
	  avatar_fragment_shader_string,
	  {},
	  shader_avatar);
}

IGL_INLINE void igl::opengl::MeshGL::free()
{
  const auto free = [](GLuint & id)
  {
    if(id)
    {
      destroy_shader_program(id);
      id = 0;
    }
  };

  if (is_initialized)
  {
    free(shader_mesh);
    free(shader_overlay_lines);
    free(shader_overlay_points);
	free(shader_stroke_points);
	free(shader_laser_points);
	free(shader_hand_point);
	free(shader_avatar);
    free_buffers();
  }
}
