#define _WIN32_WINNT 0x0601
#define NOMINMAX
#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>

// OpenGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Bullet Physics
//#include "D:/vcpkg/installed/x64-windows/include/bullet/btBulletDynamicsCommon.h"
// Bullet Physics
#include <bullet/btBulletDynamicsCommon.h>
// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Texture loading
#include <SOIL/SOIL.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable: 4005 4355 4701 4706 4244 4505 4996 4389 6001 4458)
#endif

// ===================== Global Variables =====================
int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

// Game State Management
enum GameState {
    GAME_MENU,      // Show start button
    GAME_PLAYING,   // Game is active
    GAME_ENDED      // Show score and end screen
};

GameState g_gameState = GAME_MENU;
int g_playerScore = 0;
char g_scoreGrade = 'C';
bool g_showScoreWindow = false;

// Block Selection UI
GLuint g_blockIconTextures[6] = { 0 }; // Textures for UI icons
bool g_iconsLoaded = false;

const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

// Block model files
const std::string OBJ_FILE_1 = "res\\model\\arch.obj";
const std::string OBJ_FILE_2 = "res\\model\\cone.obj";
const std::string OBJ_FILE_3 = "res\\model\\cube.obj";
const std::string OBJ_FILE_4 = "res\\model\\cylinder.obj";
const std::string OBJ_FILE_5 = "res\\model\\prism.obj";
const std::string OBJ_FILE_6 = "res\\model\\cuboid.obj";  //   

// Block scale factor - 15x size
const float BLOCK_SCALE = 2.0f;

// ===================== Error Check =====================
#define CHECK_GL_ERROR() \
    do { \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) { \
            std::cerr << "OpenGL Error: " << err << " at line " << __LINE__ << std::endl; \
        } \
    } while (0)

// ===================== Window Callback =====================
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    SCR_WIDTH = width;
    SCR_HEIGHT = (height == 0) ? 1 : height;
}

// ===================== Shader Class =====================
class Shader {
private:
    GLuint programID = 0;

    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        char infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader Compile Error:\n" << infoLog << std::endl;
        }
        return shader;
    }

public:
    bool create(const char* vertexSource, const char* fragmentSource) {
        GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        programID = glCreateProgram();
        glAttachShader(programID, vShader);
        glAttachShader(programID, fShader);
        glLinkProgram(programID);

        GLint success;
        char infoLog[512];
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(programID, 512, nullptr, infoLog);
            std::cerr << "Shader Program Link Error:\n" << infoLog << std::endl;
            return false;
        }

        glDeleteShader(vShader);
        glDeleteShader(fShader);
        return true;
    }

    bool createDepthShader(const char* vertexSource, const char* fragmentSource = nullptr) {
        GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fShader = 0;
        if (fragmentSource) {
            fShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        }

        programID = glCreateProgram();
        glAttachShader(programID, vShader);
        if (fragmentSource) {
            glAttachShader(programID, fShader);
        }
        glLinkProgram(programID);

        GLint success;
        char infoLog[512];
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(programID, 512, nullptr, infoLog);
            std::cerr << "Depth Shader Link Error:\n" << infoLog << std::endl;
            return false;
        }

        glDeleteShader(vShader);
        if (fragmentSource) glDeleteShader(fShader);
        return true;
    }

    void use() const { glUseProgram(programID); }
    void cleanup() { glDeleteProgram(programID); }

    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void setVec3(const std::string& name, const glm::vec3& vec) const {
        glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, glm::value_ptr(vec));
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
    }
    GLuint getID() const { return programID; }
};

// ===================== Texture Class =====================
class Texture {
private:
    GLuint textureID = 0;

public:
    bool load(const std::string& path) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, nrChannels;
        unsigned char* data = SOIL_load_image(path.c_str(), &width, &height, &nrChannels, SOIL_LOAD_AUTO);
        if (data) {
            GLenum format = nrChannels == 4 ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else {
            std::cerr << "Texture load failed: " << path << std::endl;
            return false;
        }
        SOIL_free_image_data(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    void createDepthTexture(unsigned int width, unsigned int height) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void bind(int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }
    void unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }
    void cleanup() { glDeleteTextures(1, &textureID); }
    GLuint getID() const { return textureID; }
};

// ===================== Framebuffer Class =====================
class Framebuffer {
private:
    GLuint fboID = 0;

public:
    bool createShadowFBO(const Texture& depthTexture) {
        glGenFramebuffers(1, &fboID);
        glBindFramebuffer(GL_FRAMEBUFFER, fboID);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture.getID(), 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Shadow FBO init failed!" << std::endl;
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fboID); }
    void unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
    void cleanup() { glDeleteFramebuffers(1, &fboID); }
};

// ===================== Block Type Enum =====================
enum GeoType {
    GEO_OBJ_1,
    GEO_OBJ_2,
    GEO_OBJ_3,
    GEO_OBJ_4,
    GEO_OBJ_5,
    GEO_OBJ_6  // 
};

// ===================== OBJ Mesh for Physics =====================
struct OBJMesh {
    std::vector<btVector3> vertices;
    std::vector<std::vector<int>> faces;
    bool isLoaded = false;
    std::string objName;
};

// Normalize OBJ mesh to fit in a 2x2x2 box (same as original code)
void normalizeOBJMesh(OBJMesh& mesh) {
    if (!mesh.isLoaded || mesh.vertices.empty()) return;
    btVector3 minV(1e20f, 1e20f, 1e20f);
    btVector3 maxV(-1e20f, -1e20f, -1e20f);
    for (const auto& v : mesh.vertices) {
        if (v.x() < minV.x()) minV.setX(v.x());
        if (v.y() < minV.y()) minV.setY(v.y());
        if (v.z() < minV.z()) minV.setZ(v.z());
        if (v.x() > maxV.x()) maxV.setX(v.x());
        if (v.y() > maxV.y()) maxV.setY(v.y());
        if (v.z() > maxV.z()) maxV.setZ(v.z());
    }
    btVector3 center = (minV + maxV) * 0.5f;
    btVector3 size = maxV - minV;
    float dimX = fabsf(size.x()), dimY = fabsf(size.y()), dimZ = fabsf(size.z());
    float maxDim = (dimX > dimY) ? (dimX > dimZ ? dimX : dimZ) : (dimY > dimZ ? dimY : dimZ);
    float scale = (maxDim > 0.0001f) ? 2.0f / maxDim : 1.0f;
    for (auto& v : mesh.vertices) {
        v = (v - center) * scale;
    }
}

// ===================== Mesh Class =====================
struct Mesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount = 0;
    std::shared_ptr<Texture> texture;
    glm::vec3 materialDiffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
    float materialTransparency = 1.0f;
    float materialRefraction = 1.5f;
    float materialSpecularIntensity = 0.15f;
    glm::vec3 materialSpecularColor = glm::vec3(1.0f);

    // Create mesh with center offset and scale to normalize position
    bool createFromAssimp(aiMesh* mesh, const aiScene* scene, const std::shared_ptr<Texture>& tex,
        float uScale = 1.0f, float vScale = 1.0f,
        glm::vec3 centerOffset = glm::vec3(0), float normScale = 1.0f) {
        texture = tex;
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        if (mesh->mMaterialIndex >= 0 && scene->mMaterials != nullptr) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
                materialDiffuseColor = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
            }
            float opacity = 1.0f;
            if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                materialTransparency = opacity;
            }
            float refraction = 1.5f;
            if (material->Get(AI_MATKEY_REFRACTI, refraction) == AI_SUCCESS) {
                materialRefraction = refraction;
            }
            aiColor3D specularColor(1.0f, 1.0f, 1.0f);
            if (material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS) {
                materialSpecularColor = glm::vec3(specularColor.r, specularColor.g, specularColor.b);
            }
            float shininess = 64.0f;
            if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
                materialSpecularIntensity = shininess / 1000.0f;
            }
        }

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            // Apply center offset and scale to normalize position (ignore original position)
            float px = (mesh->mVertices[i].x - centerOffset.x) * normScale;
            float py = (mesh->mVertices[i].y - centerOffset.y) * normScale;
            float pz = (mesh->mVertices[i].z - centerOffset.z) * normScale;
            vertices.push_back(px);
            vertices.push_back(py);
            vertices.push_back(pz);
            if (mesh->mNormals) {
                vertices.push_back(mesh->mNormals[i].x);
                vertices.push_back(mesh->mNormals[i].y);
                vertices.push_back(mesh->mNormals[i].z);
            }
            else {
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
            }
            if (mesh->mTextureCoords[0]) {
                vertices.push_back(mesh->mTextureCoords[0][i].x * uScale);
                vertices.push_back(mesh->mTextureCoords[0][i].y * vScale);
            }
            else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
            }
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }
        indexCount = static_cast<unsigned int>(indices.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    void draw(const Shader& shader) const {
        glBindVertexArray(VAO);
        if (texture) {
            texture->bind(0);
            shader.setInt("useTexture", 1);
            shader.setInt("texture1", 0);
        }
        else {
            shader.setInt("useTexture", 0);
            shader.setVec3("materialDiffuseColor", materialDiffuseColor);
        }
        shader.setFloat("materialTransparency", materialTransparency);
        shader.setFloat("materialRefraction", materialRefraction);
        shader.setFloat("materialSpecularIntensity", materialSpecularIntensity);
        shader.setVec3("materialSpecularColor", materialSpecularColor);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        if (texture) texture->unbind();
    }

    void drawDepth(const Shader& depthShader) const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        if (texture) texture->cleanup();
    }
};

// ===================== Unified Model Class =====================
class UnifiedModel {
private:
    std::vector<Mesh> meshes;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    OBJMesh objMesh;

    // Store normalization parameters for rendering mesh
    glm::vec3 meshCenterOffset = glm::vec3(0);
    float meshNormScale = 1.0f;

    void processNode(aiNode* node, const aiScene* scene, const std::shared_ptr<Texture>& texture, float uScale, float vScale) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            Mesh newMesh;
            if (newMesh.createFromAssimp(mesh, scene, texture, uScale, vScale, meshCenterOffset, meshNormScale)) {
                meshes.push_back(newMesh);
            }
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene, texture, uScale, vScale);
        }
    }

    bool loadModelCore(const std::string& modelPath, const std::shared_ptr<Texture>& texture, float uScale, float vScale, bool normalizePosition = false) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            modelPath,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_PreTransformVertices
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "Assimp Load Error: " << importer.GetErrorString() << std::endl;
            return false;
        }

        // For block models: calculate center and scale to normalize position
        if (normalizePosition && scene->mNumMeshes > 0) {
            // Calculate bounding box across ALL meshes
            glm::vec3 minV(1e20f), maxV(-1e20f);
            for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
                aiMesh* mesh = scene->mMeshes[m];
                for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                    if (mesh->mVertices[i].x < minV.x) minV.x = mesh->mVertices[i].x;
                    if (mesh->mVertices[i].y < minV.y) minV.y = mesh->mVertices[i].y;
                    if (mesh->mVertices[i].z < minV.z) minV.z = mesh->mVertices[i].z;
                    if (mesh->mVertices[i].x > maxV.x) maxV.x = mesh->mVertices[i].x;
                    if (mesh->mVertices[i].y > maxV.y) maxV.y = mesh->mVertices[i].y;
                    if (mesh->mVertices[i].z > maxV.z) maxV.z = mesh->mVertices[i].z;
                }
            }

            // Calculate center offset and normalization scale
            meshCenterOffset = (minV + maxV) * 0.5f;
            glm::vec3 size = maxV - minV;
            float maxDim = glm::max(glm::max(fabsf(size.x), fabsf(size.y)), fabsf(size.z));
            meshNormScale = (maxDim > 0.0001f) ? 2.0f / maxDim : 1.0f;

            std::cout << "Block normalized: center=(" << meshCenterOffset.x << "," << meshCenterOffset.y << "," << meshCenterOffset.z
                << ") scale=" << meshNormScale << std::endl;
        }

        // Extract physics collision data
        if (scene->mNumMeshes > 0) {
            aiMesh* firstMesh = scene->mMeshes[0];
            objMesh.vertices.clear();
            objMesh.faces.clear();

            for (unsigned int i = 0; i < firstMesh->mNumVertices; i++) {
                objMesh.vertices.push_back(btVector3(
                    firstMesh->mVertices[i].x,
                    firstMesh->mVertices[i].y,
                    firstMesh->mVertices[i].z
                ));
            }

            for (unsigned int i = 0; i < firstMesh->mNumFaces; i++) {
                aiFace face = firstMesh->mFaces[i];
                std::vector<int> faceIndices;
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    faceIndices.push_back(face.mIndices[j]);
                }
                if (faceIndices.size() >= 3) {
                    objMesh.faces.push_back(faceIndices);
                }
            }
            objMesh.isLoaded = true;
            objMesh.objName = modelPath;

            // Normalize to 2x2x2 box (same as original code)
            normalizeOBJMesh(objMesh);
        }

        processNode(scene->mRootNode, scene, texture, uScale, vScale);
        std::cout << "Model loaded: " << modelPath << " (meshes: " << meshes.size() << ")" << std::endl;
        return true;
    }

public:
    bool load(const std::string& modelPath, const std::string& texturePath, float uScale = 1.0f, float vScale = 1.0f) {
        auto texture = std::make_shared<Texture>();
        if (!texture->load(texturePath)) {
            return loadModelCore(modelPath, nullptr, uScale, vScale, false);
        }
        return loadModelCore(modelPath, texture, uScale, vScale, false);
    }

    bool load(const std::string& modelPath, float uScale = 1.0f, float vScale = 1.0f) {
        return loadModelCore(modelPath, nullptr, uScale, vScale, false);
    }

    // Load block model with position normalization (ignores original position, centers to origin)
    bool loadBlock(const std::string& modelPath) {
        return loadModelCore(modelPath, nullptr, 1.0f, 1.0f, true);
    }

    glm::mat4 getModelMatrix() const { return modelMatrix; }
    void setModelMatrix(const glm::mat4& mat) { modelMatrix = mat; }
    const OBJMesh& getOBJMesh() const { return objMesh; }

    void draw(const Shader& shader) const {
        shader.setMat4("model", modelMatrix);
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
        shader.setMat4("normalMatrix", glm::mat4(normalMatrix));
        for (const auto& mesh : meshes) {
            mesh.draw(shader);
        }
    }

    void drawWithMatrix(const Shader& shader, const glm::mat4& matrix, const glm::vec3& overrideColor, bool useOverrideColor = false) const {
        float currentScale = BLOCK_SCALE;
        if (objMesh.objName.find("arch.obj") != std::string::npos) {
            currentScale = BLOCK_SCALE * 2.0f;
        }

        // Apply block scale - 15x size
        glm::mat4 scaledMatrix = matrix * glm::scale(glm::mat4(1.0f), glm::vec3(currentScale));
        shader.setMat4("model", scaledMatrix);
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(scaledMatrix)));
        shader.setMat4("normalMatrix", glm::mat4(normalMatrix));

        if (useOverrideColor) {
            shader.setInt("useTexture", 0);
            shader.setVec3("materialDiffuseColor", overrideColor);
            shader.setFloat("materialTransparency", 1.0f);
            shader.setFloat("materialSpecularIntensity", 0.15f);
            shader.setVec3("materialSpecularColor", glm::vec3(1.0f));
        }

        for (const auto& mesh : meshes) {
            if (useOverrideColor) {
                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
            else {
                mesh.draw(shader);
            }
        }
    }

    void drawDepthWithMatrix(const Shader& depthShader, const glm::mat4& matrix) const {
        glm::mat4 scaledMatrix = matrix * glm::scale(glm::mat4(1.0f), glm::vec3(BLOCK_SCALE));
        depthShader.setMat4("model", scaledMatrix);
        for (const auto& mesh : meshes) {
            mesh.drawDepth(depthShader);
        }
    }

    void drawDepth(const Shader& depthShader) const {
        depthShader.setMat4("model", modelMatrix);
        for (const auto& mesh : meshes) {
            mesh.drawDepth(depthShader);
        }
    }

    void scale(const glm::vec3& scaleVec) {
        modelMatrix = glm::scale(modelMatrix, scaleVec);
    }

    void translate(const glm::vec3& transVec) {
        modelMatrix = glm::translate(modelMatrix, transVec);
    }

    void rotate(float angle, const glm::vec3& axis) {
        modelMatrix = glm::rotate(modelMatrix, glm::radians(angle), axis);
    }

    void cleanup() {
        for (auto& mesh : meshes) {
            mesh.cleanup();
        }
        meshes.clear();
    }
};

// ===================== Light Management =====================
struct DirectionalLight {
    glm::vec3 direction = glm::vec3(-1.0f, -2.0f, -2.0f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 0.55f;
};

struct PointLight {
    glm::vec3 position = glm::vec3(29.0f, 20.0f, 14.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 2.0f;
    float constant = 1.0f;
    float linear = 0.07f;
    float quadratic = 0.02f;
};

class LightManager {
private:
    glm::vec3 ambientLight = glm::vec3(0.55f);
    std::vector<PointLight> pointLights;
    DirectionalLight dirLight;

public:
    void addPointLight(const PointLight& light) { pointLights.push_back(light); }
    void setAmbientLight(const glm::vec3& ambient) { ambientLight = ambient; }
    void setDirectionalLight(const DirectionalLight& light) { dirLight = light; }
    DirectionalLight getDirectionalLight() const { return dirLight; }

    void sendToShader(const Shader& shader) const {
        shader.setVec3("ambientLight", ambientLight);
        shader.setVec3("dirLight.direction", dirLight.direction);
        shader.setVec3("dirLight.color", dirLight.color);
        shader.setFloat("dirLight.intensity", dirLight.intensity);

        int lightCount = std::min((int)pointLights.size(), 4);
        shader.setInt("pointLightCount", lightCount);
        for (int i = 0; i < lightCount; i++) {
            std::string prefix = "pointLights[" + std::to_string(i) + "]";
            shader.setVec3(prefix + ".position", pointLights[i].position);
            shader.setVec3(prefix + ".color", pointLights[i].color);
            shader.setFloat(prefix + ".intensity", pointLights[i].intensity);
            shader.setFloat(prefix + ".constant", pointLights[i].constant);
            shader.setFloat(prefix + ".linear", pointLights[i].linear);
            shader.setFloat(prefix + ".quadratic", pointLights[i].quadratic);
        }
        shader.setFloat("specularStrength", 0.15f);
        shader.setInt("shininess", 64);
    }
};

// ===================== Camera System =====================
class Camera {
private:
    const float CAMERA_SPEED = 10.0f;
    const float CAMERA_SPRINT_SPEED = 20.0f;
    const float MOUSE_SENSITIVITY = 0.1f;
    const float MAX_PITCH = 89.0f;

    glm::vec3 position = glm::vec3(27.0f, 12.0f, 80.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool isRightMousePressed = false;

    void updateFront() {
        front.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front.y = sinf(glm::radians(pitch));
        front.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front = glm::normalize(front);
    }

public:
    float deltaTime = 0.0f;

    void processInput(GLFWwindow* window) {
        // Only process camera input when game is playing
        if (g_gameState != GAME_PLAYING) {
            return;
        }

        float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? CAMERA_SPRINT_SPEED : CAMERA_SPEED;
        speed *= deltaTime;

        glm::vec3 horizontalFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
        glm::vec3 horizontalRight = glm::normalize(glm::cross(horizontalFront, up));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += horizontalFront * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= horizontalFront * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= horizontalRight * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += horizontalRight * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) position.y -= speed;

        position.y = glm::max(position.y, 0.5f);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 1000.0f) const {
        float aspectRatio = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    bool isRightPressed() const { return isRightMousePressed; }
    void setRightPressed(bool pressed) { isRightMousePressed = pressed; }
    void setLastPos(float x, float y) { lastX = x; lastY = y; }
    float getLastX() const { return lastX; }
    float getLastY() const { return lastY; }
    void setYaw(float y) { yaw = y; updateFront(); }
    void setPitch(float p) { pitch = glm::clamp(p, -MAX_PITCH, MAX_PITCH); updateFront(); }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    float getSensitivity() const { return MOUSE_SENSITIVITY; }
    float getMaxPitch() const { return MAX_PITCH; }
};

// ===================== Score System =====================
void updateScoreGrade() {
    if (g_playerScore >= 20) {
        g_scoreGrade = 'A';
    }
    else if (g_playerScore >= 10) {
        g_scoreGrade = 'B';
    }
    else {
        g_scoreGrade = 'C';
    }
}

void incrementScore() {
    if (g_gameState == GAME_PLAYING) {
        g_playerScore++;
        updateScoreGrade();
        std::cout << "[Score] Current score: " << g_playerScore << " (Grade: " << g_scoreGrade << ")" << std::endl;
    }
}

void resetScore() {
    g_playerScore = 0;
    g_scoreGrade = 'C';
    updateScoreGrade();
}

// ===================== UI Texture Loading =====================
bool loadBlockIconTextures() {
    // Block icon file names - adjust these based on your actual file names
    // Option 1: Use numbered names (block1.png, block2.png, etc.)
    std::vector<std::string> iconFiles = {
        "res/texture/arch.jpg",  // GEO_OBJ_1 - arch
        "res/texture/cone.jpg",  // GEO_OBJ_2 - cone  
        "res/texture/cube.jpg",  // GEO_OBJ_3 - cube
        "res/texture/cylinder.jpg",  // GEO_OBJ_4 - cylinder
        "res/texture/prism.jpg",  // GEO_OBJ_5 - prism
        "res/texture/cuboid.jpg"   // GEO_OBJ_6 - cuboid
    };


    for (int i = 0; i < 6; i++) {
        int width, height, nrChannels;
        unsigned char* data = SOIL_load_image(iconFiles[i].c_str(), &width, &height, &nrChannels, SOIL_LOAD_AUTO);

        if (data) {
            glGenTextures(1, &g_blockIconTextures[i]);
            glBindTexture(GL_TEXTURE_2D, g_blockIconTextures[i]);

            GLenum format = nrChannels == 4 ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            SOIL_free_image_data(data);
            std::cout << "Loaded block icon: " << iconFiles[i] << std::endl;
        }
        else {
            std::cerr << "Failed to load block icon: " << iconFiles[i] << std::endl;
            // Create a fallback colored texture
            glGenTextures(1, &g_blockIconTextures[i]);
            glBindTexture(GL_TEXTURE_2D, g_blockIconTextures[i]);

            // Create a solid color texture as fallback
            unsigned char color[] = {
                (unsigned char)(50 + i * 30),   // R
                (unsigned char)(100 + i * 20),  // G
                (unsigned char)(150 + i * 10),  // B
                255                             // A
            };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    g_iconsLoaded = true;
    return true;
}

void cleanupBlockIconTextures() {
    for (int i = 0; i < 6; i++) {
        if (g_blockIconTextures[i] != 0) {
            glDeleteTextures(1, &g_blockIconTextures[i]);
            g_blockIconTextures[i] = 0;
        }
    }
    g_iconsLoaded = false;
}

// ===================== ImGui UI System =====================
// Function declaration - definition will be after global variables
void renderUI();

// ===================== Physics World (Following original code exactly) =====================
struct GeoData {
    btRigidBody* body;
    GeoType type;
    glm::vec3 color;
    btCollisionShape* shape;
    int modelIndex;
    GeoData(btRigidBody* b, GeoType t, const glm::vec3& c, btCollisionShape* s, int idx)
        : body(b), type(t), color(c), shape(s), modelIndex(idx) {
    }
};

class PhysicsWorld {
private:
    btDiscreteDynamicsWorld* dynamicsWorld = nullptr;
    btDefaultCollisionConfiguration* collisionConfig = nullptr;
    btCollisionDispatcher* dispatcher = nullptr;
    btBroadphaseInterface* broadphase = nullptr;
    btSequentialImpulseConstraintSolver* solver = nullptr;
    btRigidBody* groundRigidBody = nullptr;

public:
    std::vector<GeoData> geometries;

    // From original code - exact same variable names
    btRigidBody* selectedGeo = nullptr;
    bool isFollow = false;
    btVector3 initPos;
    float verticalHeight = 5.0f;
    float followSmoothFactor = 0.05f;  // Slower, smoother following
    btVector3 horizontalPos;

    // Bounds from original code
    const float MIN_X = -150.0f, MAX_X = 150.0f;
    const float MIN_Z = -150.0f, MAX_Z = 150.0f;
    const float MIN_Y = 2.0f, MAX_Y = 50.0f;
    const float DEFAULT_SPAWN_HEIGHT = 10.0f;

    // Mouse position for following
    int camLastMouseX = 0;
    int camLastMouseY = 0;

    GeoType g_nextGeoType = GEO_OBJ_1;

    bool init() {
        // From original initPhysics() - exactly the same
        collisionConfig = new btDefaultCollisionConfiguration();
        dispatcher = new btCollisionDispatcher(collisionConfig);
        broadphase = new btDbvtBroadphase();
        solver = new btSequentialImpulseConstraintSolver();
        dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);
        dynamicsWorld->setGravity(btVector3(0.0f, -9.8f, 0.0f));
        dynamicsWorld->getSolverInfo().m_numIterations = 100;

        // Ground at y=-1, surface at y=0 (same as original: btVector3(50,1,50) at y=-1)
        // Enlarged to cover room floor
        btCollisionShape* groundShape = new btBoxShape(btVector3(500.0f, 1.0f, 500.0f));
        btDefaultMotionState* groundMS = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
        btRigidBody::btRigidBodyConstructionInfo groundCI(0, groundMS, groundShape, btVector3(0, 0, 0));
        groundRigidBody = new btRigidBody(groundCI);
        groundRigidBody->setFriction(2.0f);         // High friction ground
        groundRigidBody->setRestitution(0.0f);      // No bounce
        groundRigidBody->setCollisionFlags(groundRigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        dynamicsWorld->addRigidBody(groundRigidBody);

        return true;
    }

    glm::vec3 getRandomColor() {
        return glm::vec3(
            (rand() % 80 + 20) / 100.0f,
            (rand() % 80 + 20) / 100.0f,
            (rand() % 80 + 20) / 100.0f
        );
    }

    // From original createGeoShape()
    btCollisionShape* createGeoShape(GeoType type, const std::vector<UnifiedModel*>& blockModels) {
        if (type >= 0 && (size_t)type < blockModels.size() && blockModels[type]) {
            const OBJMesh& objMesh = blockModels[type]->getOBJMesh();
            if (objMesh.isLoaded && !objMesh.vertices.empty()) {
                btConvexHullShape* objShape = new btConvexHullShape();
                for (const auto& v : objMesh.vertices) {
                    // Scale vertices by BLOCK_SCALE (15x)
                    objShape->addPoint(v * BLOCK_SCALE);
                }
                objShape->optimizeConvexHull();
                return objShape;
            }
        }
        return new btBoxShape(btVector3(1.0f * BLOCK_SCALE, 1.0f * BLOCK_SCALE, 1.0f * BLOCK_SCALE));
    }

    // From original addGeometry() with same physics parameters
    void addGeometry(GeoType type, const btVector3& spawnPos, const glm::vec3& color, const std::vector<UnifiedModel*>& blockModels) {
        btCollisionShape* geoShape = createGeoShape(type, blockModels);
        btScalar mass = 2.0f;
        btVector3 inertia(0, 0, 0);
        if (mass > 0) geoShape->calculateLocalInertia(mass, inertia);

        btDefaultMotionState* geoMS = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), spawnPos));
        btRigidBody::btRigidBodyConstructionInfo geoCI(mass, geoMS, geoShape, inertia);
        // Realistic physics parameters for wooden blocks
        geoCI.m_friction = 1.8f;           // High friction - wooden blocks don't slide easily
        geoCI.m_restitution = 0.05f;       // Very low bounce - blocks don't bounce much
        geoCI.m_linearDamping = 0.5f;      // Medium linear damping
        geoCI.m_angularDamping = 0.8f;     // High angular damping - blocks don't spin forever

        btRigidBody* geo = new btRigidBody(geoCI);
        geo->setActivationState(DISABLE_DEACTIVATION);
        geo->setSleepingThresholds(0.1f, 0.1f);  // Allow sleeping when stable
        geo->setContactProcessingThreshold(0.01f);
        // CCD parameters
        geo->setCcdMotionThreshold(0.1f);
        geo->setCcdSweptSphereRadius(0.5f * BLOCK_SCALE);

        dynamicsWorld->addRigidBody(geo);
        geometries.emplace_back(geo, type, color, geoShape, (int)type);

        std::string modelName;
        switch (type) {
        case GEO_OBJ_1: modelName = OBJ_FILE_1; break;
        case GEO_OBJ_2: modelName = OBJ_FILE_2; break;
        case GEO_OBJ_3: modelName = OBJ_FILE_3; break;
        case GEO_OBJ_4: modelName = OBJ_FILE_4; break;
        case GEO_OBJ_5: modelName = OBJ_FILE_5; break;
        case GEO_OBJ_6: modelName = OBJ_FILE_6; break;  //      Ļ ľ    
        }
        std::cout << "[Generated] " << modelName << " at (" << spawnPos.x() << "," << spawnPos.y() << "," << spawnPos.z() << ")" << std::endl;
    }

    // Ray casting for picking objects
    btRigidBody* pickGeometry(double mouseX, double mouseY, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos) {
        // Convert mouse to normalized device coordinates
        float ndcX = (2.0f * (float)mouseX) / (float)SCR_WIDTH - 1.0f;
        float ndcY = 1.0f - (2.0f * (float)mouseY) / (float)SCR_HEIGHT;

        // Near and far points in NDC
        glm::vec4 rayStartNDC(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);

        // Transform to world space
        glm::mat4 invVP = glm::inverse(projection * view);

        glm::vec4 rayStartWorld = invVP * rayStartNDC;
        rayStartWorld /= rayStartWorld.w;

        glm::vec4 rayEndWorld = invVP * rayEndNDC;
        rayEndWorld /= rayEndWorld.w;

        glm::vec3 rayDir = glm::normalize(glm::vec3(rayEndWorld - rayStartWorld));

        btVector3 from(camPos.x, camPos.y, camPos.z);
        btVector3 to = from + btVector3(rayDir.x, rayDir.y, rayDir.z) * 1000.0f;

        btCollisionWorld::ClosestRayResultCallback callback(from, to);
        dynamicsWorld->rayTest(from, to, callback);

        if (callback.hasHit()) {
            btCollisionObject* obj = const_cast<btCollisionObject*>(callback.m_collisionObject);
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body != groundRigidBody) {
                return body;
            }
        }
        return nullptr;
    }

    // Simple spawn position: in front of camera at specified height
    btVector3 getSpawnInFrontOfCamera(const glm::vec3& camPos, const glm::vec3& camFront, float distance = 20.0f) {
        // Get horizontal direction (ignore Y component of camera front)
        glm::vec3 horizontalFront = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));

        // Spawn position: in front of camera, at spawn height
        float spawnX = camPos.x + horizontalFront.x * distance;
        float spawnZ = camPos.z + horizontalFront.z * distance;

        return btVector3(spawnX, DEFAULT_SPAWN_HEIGHT, spawnZ);
    }

    // Screen to world for dragging (horizontal plane at current height)
    btVector3 screenToWorldAtHeight(double mouseX, double mouseY, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, float targetY) {
        // Convert mouse to normalized device coordinates
        float ndcX = (2.0f * (float)mouseX) / (float)SCR_WIDTH - 1.0f;
        float ndcY = 1.0f - (2.0f * (float)mouseY) / (float)SCR_HEIGHT;

        // Near and far points in NDC
        glm::vec4 rayStartNDC(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);

        // Transform to world space
        glm::mat4 invVP = glm::inverse(projection * view);

        glm::vec4 rayStartWorld = invVP * rayStartNDC;
        rayStartWorld /= rayStartWorld.w;

        glm::vec4 rayEndWorld = invVP * rayEndNDC;
        rayEndWorld /= rayEndWorld.w;

        glm::vec3 rayDir = glm::normalize(glm::vec3(rayEndWorld - rayStartWorld));

        // Intersect with y=targetY plane
        if (fabsf(rayDir.y) > 0.0001f) {
            float t = (targetY - camPos.y) / rayDir.y;
            if (t > 0 && t < 500.0f) {
                float hitX = camPos.x + rayDir.x * t;
                float hitZ = camPos.z + rayDir.z * t;
                hitX = glm::clamp(hitX, MIN_X, MAX_X);
                hitZ = glm::clamp(hitZ, MIN_Z, MAX_Z);
                return btVector3(hitX, targetY, hitZ);
            }
        }

        // Fallback
        return btVector3(camPos.x, targetY, camPos.z);
    }

    // From original pauseGeoPhysics() - EXACT copy
    void pauseGeoPhysics(btRigidBody* geo) {
        if (!geo) return;
        btTransform trans;
        geo->getMotionState()->getWorldTransform(trans);
        initPos = trans.getOrigin();
        horizontalPos = btVector3(initPos.getX(), 0, initPos.getZ());
        verticalHeight = initPos.getY() + 1.0f;
        geo->clearForces();
        geo->setLinearVelocity(btVector3(0, 0, 0));
        geo->setAngularVelocity(btVector3(0, 0, 0));
        geo->setCollisionFlags(geo->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        geo->setActivationState(DISABLE_DEACTIVATION);
    }

    // From original resumeGeoPhysics() - EXACT copy
    void resumeGeoPhysics(btRigidBody* geo) {
        if (!geo) return;
        geo->setLinearVelocity(btVector3(0, -0.05f, 0));
        geo->setAngularVelocity(btVector3(0, 0, 0));
        geo->clearForces();
        geo->setCollisionFlags(geo->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        geo->setActivationState(ACTIVE_TAG);
    }

    // Update following object position (from original updateScene)
    void updateFollowing(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos) {
        if (isFollow && selectedGeo) {
            btVector3 targetWorldPos = screenToWorldAtHeight(camLastMouseX, camLastMouseY, view, projection, camPos, verticalHeight);

            btTransform trans;
            selectedGeo->getMotionState()->getWorldTransform(trans);
            btVector3 currentPos = trans.getOrigin();

            // Smooth following with slower speed for more control
            btVector3 newPos = currentPos.lerp(targetWorldPos, followSmoothFactor);
            newPos.setY(verticalHeight); // Keep at current height

            trans.setOrigin(newPos);
            selectedGeo->setWorldTransform(trans);
            selectedGeo->getMotionState()->setWorldTransform(trans);
            horizontalPos = btVector3(newPos.x(), 0, newPos.z());
        }
    }

    void update(float deltaTime) {
        if (dynamicsWorld) {
            // Same as original: stepSimulation(1.0/60.0, 20, 1.0/120.0)
            dynamicsWorld->stepSimulation(1.0f / 60.0f, 20, 1.0f / 120.0f);
        }
    }

    // Clear all blocks but keep the physics world running
    void clearAllBlocks() {
        // Reset selection state
        selectedGeo = nullptr;
        isFollow = false;

        // Remove all geometries from physics world and free memory
        for (size_t i = 0; i < geometries.size(); i++) {
            dynamicsWorld->removeRigidBody(geometries[i].body);
            delete geometries[i].body->getMotionState();
            delete geometries[i].shape;
            delete geometries[i].body;
        }
        geometries.clear();

        std::cout << "[Game] All blocks cleared for new game" << std::endl;
    }

    void cleanup() {
        for (size_t i = 0; i < geometries.size(); i++) {
            dynamicsWorld->removeRigidBody(geometries[i].body);
            delete geometries[i].body->getMotionState();
            delete geometries[i].shape;
            delete geometries[i].body;
        }
        geometries.clear();

        if (groundRigidBody) {
            dynamicsWorld->removeRigidBody(groundRigidBody);
            delete groundRigidBody->getMotionState();
            delete groundRigidBody->getCollisionShape();
            delete groundRigidBody;
        }

        delete dynamicsWorld;
        delete solver;
        delete dispatcher;
        delete broadphase;
        delete collisionConfig;
    }
};

// ===================== Forward Declarations =====================
class PhysicsWorld;
class Camera;
class UnifiedModel;

// ===================== Global Pointers =====================
PhysicsWorld* g_physicsWorld = nullptr;
Camera* g_camera = nullptr;
std::vector<UnifiedModel*>* g_blockModelPtrs = nullptr;
bool g_leftMouseDown = false;

// ===================== UI Implementation =====================
void renderUI() {
    // Check if required globals are available
    if (!g_physicsWorld || !g_camera || !g_blockModelPtrs) {
        // Only show basic menu if globals not ready
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowPos(ImVec2((float)(SCR_WIDTH - 280), 10.0f), ImGuiCond_Always);
        ImGui::Begin("Game Control", nullptr, window_flags);
        ImGui::Text("Loading...");
        ImGui::End();
        return;
    }
    // Set ImGui window flags for fixed position
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

    // Main game UI - top right corner (larger)
    ImGui::SetNextWindowPos(ImVec2((float)(SCR_WIDTH - 280), 10.0f), ImGuiCond_Always);
    ImGui::Begin("Game Control", nullptr, window_flags);

    switch (g_gameState) {
    case GAME_MENU:
        ImGui::Text("Block Building Game");
        ImGui::Separator();
        if (ImGui::Button("START GAME", ImVec2(240.0f, 50.0f))) {
            g_gameState = GAME_PLAYING;
            resetScore();
            // Clear all existing blocks from previous games
            if (g_physicsWorld) {
                g_physicsWorld->clearAllBlocks();
            }
            std::cout << "[Game] Game started!" << std::endl;
        }
        break;

    case GAME_PLAYING:
        ImGui::Text("Game Active");
        ImGui::Separator();
        ImGui::Text("Score: %d", g_playerScore);
        ImGui::Text("Grade: %c", g_scoreGrade);
        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("1-6: Create blocks");
        ImGui::Text("Click: Select/Release");
        ImGui::Text("T/B: Move up/down");
        ImGui::Text("R: Reset rotation");
        ImGui::Separator();
        if (ImGui::Button("END GAME", ImVec2(240.0f, 50.0f))) {
            g_gameState = GAME_ENDED;
            g_showScoreWindow = true;
            std::cout << "[Game] Game ended!" << std::endl;
        }
        break;

    case GAME_ENDED:
        ImGui::Text("Game Ended");
        ImGui::Separator();
        ImGui::Text("Final Score: %d", g_playerScore);
        ImGui::Text("Final Grade: %c", g_scoreGrade);
        ImGui::Separator();
        if (ImGui::Button("NEW GAME", ImVec2(240.0f, 50.0f))) {
            g_gameState = GAME_MENU;
            g_showScoreWindow = false;
            std::cout << "[Game] Ready for new game!" << std::endl;
        }
        break;
    }

    ImGui::End();

    // Score result window (popup style)
    if (g_showScoreWindow && g_gameState == GAME_ENDED) {
        ImGui::SetNextWindowPos(ImVec2((float)(SCR_WIDTH / 2 - 200), (float)(SCR_HEIGHT / 2 - 120)), ImGuiCond_Always);
        ImGui::Begin("Final Score", nullptr, window_flags);

        ImGui::Text("Game Complete!");
        ImGui::Separator();
        ImGui::Text("Blocks Created: %d", g_playerScore);
        ImGui::Text("Final Grade: %c", g_scoreGrade);
        ImGui::Separator();

        // Grade evaluation
        if (g_scoreGrade == 'A') {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Excellent! ");
        }
        else if (g_scoreGrade == 'B') {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Good! ");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Keep practicing! ");
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(160.0f, 40.0f))) {
            g_showScoreWindow = false;
        }

        ImGui::End();
    }

    // Block selection toolbar (bottom of screen) - Extra Large
    if (g_gameState == GAME_PLAYING && g_iconsLoaded) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, (float)(SCR_HEIGHT - 160)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(760.0f, 150.0f), ImGuiCond_Always);

        ImGuiWindowFlags toolbar_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("BlockToolbar", nullptr, toolbar_flags);

        ImGui::Text("Block Selection (Click icons or press 1-6):");
        ImGui::Separator();

        // Create 6 extra large image buttons in a row
        const float buttonSize = 85.0f;
        const char* blockNames[] = { "Arch", "Cone", "Cube", "Cylinder", "Prism", "Cuboid" };

        for (int i = 0; i < 6; i++) {
            if (i > 0) ImGui::SameLine();

            ImGui::BeginGroup();

            // Image button with texture
            bool clicked = false;
            if (g_blockIconTextures[i] != 0) {
                // Create unique ID string for each button
                std::string buttonId = "block_" + std::to_string(i);

                // Try newer ImGui API first (with string ID)
#if IMGUI_VERSION_NUM >= 18700  // ImGui 1.87+
                clicked = ImGui::ImageButton(
                    buttonId.c_str(),
                    (ImTextureID)(intptr_t)g_blockIconTextures[i],
                    ImVec2(buttonSize, buttonSize),
                    ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                    ImVec4(0.0f, 0.0f, 0.0f, 0.0f),  // bg color
                    ImVec4(1.0f, 1.0f, 1.0f, 1.0f)   // tint color
                );
#else  // Older ImGui API
                ImGui::PushID(i);
                clicked = ImGui::ImageButton(
                    (ImTextureID)(intptr_t)g_blockIconTextures[i],
                    ImVec2(buttonSize, buttonSize),
                    ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                    1,  // border
                    ImVec4(0.0f, 0.0f, 0.0f, 0.0f),  // bg color
                    ImVec4(1.0f, 1.0f, 1.0f, 1.0f)   // tint color
                );
                ImGui::PopID();
#endif
            }
            else {
                // Fallback text button if texture failed to load (larger)
                clicked = ImGui::Button(std::to_string(i + 1).c_str(), ImVec2(buttonSize, buttonSize));
            }

            // Handle click - spawn the corresponding block
            if (clicked && g_physicsWorld && g_camera && g_blockModelPtrs) {
                GeoType blockType = (GeoType)i;
                g_physicsWorld->g_nextGeoType = blockType;

                // Spawn in front of camera
                glm::vec3 camPos = g_camera->getPosition();
                glm::vec3 camFront = g_camera->getFront();
                btVector3 spawnPos = g_physicsWorld->getSpawnInFrontOfCamera(camPos, camFront, 25.0f);
                g_physicsWorld->addGeometry(blockType, spawnPos, g_physicsWorld->getRandomColor(), *g_blockModelPtrs);

                // Increment score
                incrementScore();

                std::cout << "[UI] Generated " << blockNames[i] << " block (type " << (i + 1) << ")" << std::endl;
            }

            // Tooltip and key hint
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n(Press %d)", blockNames[i], i + 1);
            }

            // Show key number below button
            ImGui::Text(" %d", i + 1);

            ImGui::EndGroup();
        }

        ImGui::End();
    }
}

// ===================== Input Callbacks =====================
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Let ImGui handle input first
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (!g_physicsWorld || !g_camera || !g_blockModelPtrs) return;

    // Only allow game controls when game is active
    if (g_gameState != GAME_PLAYING) {
        // ESC key always works to close game
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    // Press 1-6 to generate blocks (      6)
    if (action == GLFW_PRESS) {
        GeoType newType = GEO_OBJ_1;
        bool shouldSpawn = false;

        switch (key) {
        case GLFW_KEY_1: newType = GEO_OBJ_1; shouldSpawn = true; break;
        case GLFW_KEY_2: newType = GEO_OBJ_2; shouldSpawn = true; break;
        case GLFW_KEY_3: newType = GEO_OBJ_3; shouldSpawn = true; break;
        case GLFW_KEY_4: newType = GEO_OBJ_4; shouldSpawn = true; break;
        case GLFW_KEY_5: newType = GEO_OBJ_5; shouldSpawn = true; break;
        case GLFW_KEY_6: newType = GEO_OBJ_6; shouldSpawn = true; break;  //      İ   
        }

        if (shouldSpawn) {
            g_physicsWorld->g_nextGeoType = newType;
            // Spawn in front of camera
            glm::vec3 camPos = g_camera->getPosition();
            glm::vec3 camFront = g_camera->getFront();
            btVector3 spawnPos = g_physicsWorld->getSpawnInFrontOfCamera(camPos, camFront, 25.0f);
            g_physicsWorld->addGeometry(newType, spawnPos, g_physicsWorld->getRandomColor(), *g_blockModelPtrs);
            // Increment score when block is created
            incrementScore();
        }
    }

    // T = move up, B = move down (when holding a block) - replaces original W/S
    if (g_physicsWorld->isFollow && g_physicsWorld->selectedGeo) {
        if (key == GLFW_KEY_T && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            g_physicsWorld->verticalHeight += 0.5f;
            if (g_physicsWorld->verticalHeight > g_physicsWorld->MAX_Y)
                g_physicsWorld->verticalHeight = g_physicsWorld->MAX_Y;

            btTransform trans;
            g_physicsWorld->selectedGeo->getMotionState()->getWorldTransform(trans);
            trans.setOrigin(btVector3(g_physicsWorld->horizontalPos.getX(), g_physicsWorld->verticalHeight, g_physicsWorld->horizontalPos.getZ()));
            g_physicsWorld->selectedGeo->setWorldTransform(trans);
            g_physicsWorld->selectedGeo->getMotionState()->setWorldTransform(trans);
        }
        if (key == GLFW_KEY_B && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            g_physicsWorld->verticalHeight -= 0.5f;
            if (g_physicsWorld->verticalHeight < g_physicsWorld->MIN_Y)
                g_physicsWorld->verticalHeight = g_physicsWorld->MIN_Y;

            btTransform trans;
            g_physicsWorld->selectedGeo->getMotionState()->getWorldTransform(trans);
            trans.setOrigin(btVector3(g_physicsWorld->horizontalPos.getX(), g_physicsWorld->verticalHeight, g_physicsWorld->horizontalPos.getZ()));
            g_physicsWorld->selectedGeo->setWorldTransform(trans);
            g_physicsWorld->selectedGeo->getMotionState()->setWorldTransform(trans);
        }

        // R = reset block rotation to upright position (fix fallen blocks)
        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            btTransform trans;
            g_physicsWorld->selectedGeo->getMotionState()->getWorldTransform(trans);
            // Reset rotation to identity (upright), keep position
            trans.setRotation(btQuaternion(0, 0, 0, 1));
            g_physicsWorld->selectedGeo->setWorldTransform(trans);
            g_physicsWorld->selectedGeo->getMotionState()->setWorldTransform(trans);
            std::cout << "[Reset] Block rotation reset to upright" << std::endl;
        }
    }
}

// Mouse button callback - FOLLOWING ORIGINAL mouseClick() EXACTLY
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // Let ImGui handle input first
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (!g_physicsWorld || !g_camera) return;

    // Only allow game controls when game is active
    if (g_gameState != GAME_PLAYING) {
        return;
    }

    // Right mouse for camera rotation
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_camera->setRightPressed(action == GLFW_PRESS);
        if (action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            g_camera->setLastPos((float)x, (float)y);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // Left mouse - FROM ORIGINAL mouseClick()
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        g_physicsWorld->camLastMouseX = (int)mouseX;
        g_physicsWorld->camLastMouseY = (int)mouseY;

        glm::mat4 view = g_camera->getViewMatrix();
        glm::mat4 projection = g_camera->getProjectionMatrix();

        if (!g_physicsWorld->selectedGeo) {
            // Try to pick object (from original mouseClick)
            glm::vec3 camPos = g_camera->getPosition();
            btRigidBody* picked = g_physicsWorld->pickGeometry(mouseX, mouseY, view, projection, camPos);
            if (picked) {
                g_physicsWorld->selectedGeo = picked;
                g_physicsWorld->isFollow = true;
                g_physicsWorld->pauseGeoPhysics(g_physicsWorld->selectedGeo);
                std::cout << "[Selected] Block - drag to move, T=up, B=down, click to release" << std::endl;
            }
        }
        else {
            // Release object (from original mouseClick)
            g_physicsWorld->resumeGeoPhysics(g_physicsWorld->selectedGeo);
            g_physicsWorld->isFollow = false;
            g_physicsWorld->selectedGeo = nullptr;
            std::cout << "[Released] Block" << std::endl;
        }
        g_leftMouseDown = true;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        g_leftMouseDown = false;
    }
}

// Cursor position callback - FROM ORIGINAL mouseMove()
void cursorPosCallback(GLFWwindow* window, double xposIn, double yposIn) {
    // Let ImGui handle input first
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (!g_camera || !g_physicsWorld) return;

    // Only allow game controls when game is active
    if (g_gameState != GAME_PLAYING) {
        return;
    }

    // Update mouse position for following
    g_physicsWorld->camLastMouseX = (int)xposIn;
    g_physicsWorld->camLastMouseY = (int)yposIn;

    // Camera rotation with right mouse
    if (g_camera->isRightPressed()) {
        float xpos = (float)xposIn;
        float ypos = (float)yposIn;

        float xoffset = xpos - g_camera->getLastX();
        float yoffset = g_camera->getLastY() - ypos;
        g_camera->setLastPos(xpos, ypos);

        xoffset *= g_camera->getSensitivity();
        yoffset *= g_camera->getSensitivity();

        g_camera->setYaw(g_camera->getYaw() + xoffset);
        g_camera->setPitch(g_camera->getPitch() + yoffset);
    }
}

// ===================== Main Program =====================
int main() {
    srand((unsigned int)time(NULL));

    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW init failed" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Block Building Game", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window create failed" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // 2. Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW init failed" << std::endl;
        return -1;
    }

    // 3. OpenGL Configuration
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 4. Shadow Resources
    Texture shadowMapTexture;
    shadowMapTexture.createDepthTexture(SHADOW_WIDTH, SHADOW_HEIGHT);
    Framebuffer shadowFBO;
    if (!shadowFBO.createShadowFBO(shadowMapTexture)) {
        glfwTerminate();
        return -1;
    }

    // 5. Initialize Camera
    Camera camera;
    g_camera = &camera;
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // 6. Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style - Light Yellow Theme with Larger UI
    ImGui::StyleColorsDark(); // Start with dark as base

    // Customize colors and sizing to light yellow theme with larger elements
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ===== FONT AND SIZE SCALING =====
    io.FontGlobalScale = 1.4f;  // Scale font size by 140%

    // ===== UI ELEMENT SIZING =====
    style.WindowPadding = ImVec2(12.0f, 12.0f);        // Larger window padding
    style.WindowRounding = 8.0f;                        // More rounded corners
    style.WindowMinSize = ImVec2(100.0f, 100.0f);      // Larger minimum window size

    style.FramePadding = ImVec2(8.0f, 6.0f);           // Larger button/frame padding
    style.FrameRounding = 6.0f;                         // More rounded frames

    style.ItemSpacing = ImVec2(12.0f, 8.0f);           // More space between items
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);       // More inner spacing

    style.ScrollbarSize = 20.0f;                        // Larger scrollbars
    style.ScrollbarRounding = 10.0f;

    style.GrabMinSize = 12.0f;                          // Larger grab handles
    style.GrabRounding = 6.0f;

    // ===== BORDER AND VISUAL ELEMENTS =====
    style.WindowBorderSize = 2.0f;                      // Thicker window borders
    style.FrameBorderSize = 1.0f;                       // Visible frame borders
    style.PopupBorderSize = 2.0f;                       // Thicker popup borders

    style.IndentSpacing = 24.0f;                        // Larger indent spacing
    style.ColumnsMinSpacing = 8.0f;                     // More column spacing

    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);        // Center button text
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);    // Center selectable text vertically

    // Main window colors - light yellow background
    colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 0.9f, 0.95f);        // Light cream background
    colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 0.85f, 0.95f);        // Popup background
    colors[ImGuiCol_ChildBg] = ImVec4(1.0f, 1.0f, 0.9f, 0.0f);          // Child window background

    // Text colors
    colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.1f, 1.0f);             // Dark text for readability
    colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.4f, 1.0f);     // Disabled text

    // Button colors - yellow theme
    colors[ImGuiCol_Button] = ImVec4(1.0f, 0.9f, 0.6f, 1.0f);           // Light yellow buttons
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);   // Hover state
    colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);     // Active/pressed state

    // Header colors (for collapsible headers, etc.)
    colors[ImGuiCol_Header] = ImVec4(1.0f, 0.9f, 0.7f, 0.8f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.85f, 0.5f, 0.8f);
    colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.8f, 0.3f, 0.8f);

    // Frame colors (for input boxes, etc.)
    colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 0.8f, 0.8f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 0.95f, 0.7f, 0.8f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 0.9f, 0.6f, 0.8f);

    // Title bar colors
    colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.9f, 0.6f, 0.8f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.85f, 0.4f, 0.8f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.95f, 0.7f, 0.5f);

    // Border colors
    colors[ImGuiCol_Border] = ImVec4(0.8f, 0.7f, 0.4f, 0.6f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Separator colors
    colors[ImGuiCol_Separator] = ImVec4(0.7f, 0.6f, 0.3f, 0.6f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.8f, 0.7f, 0.4f, 0.8f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.9f, 0.8f, 0.5f, 1.0f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    std::cout << "ImGui initialized successfully with light yellow theme" << std::endl;

    // 7. Load Block Icon Textures for UI
    if (!loadBlockIconTextures()) {
        std::cerr << "Warning: Some block icons failed to load" << std::endl;
    }

    // 8. Create Shaders
    Shader depthShader;
    const char* depthVS =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 model;\n"
        "uniform mat4 lightSpaceMatrix;\n"
        "void main() {\n"
        "    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);\n"
        "}";
    if (!depthShader.createDepthShader(depthVS)) {
        glfwTerminate();
        return -1;
    }

    Shader shader;
    const char* mainVS =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "layout (location = 2) in vec2 aTexCoord;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform mat4 normalMatrix;\n"
        "uniform mat4 lightSpaceMatrix;\n"
        "out vec3 fragPos;\n"
        "out vec3 fragNormal;\n"
        "out vec2 texCoord;\n"
        "out vec4 fragPosLightSpace;\n"
        "void main() {\n"
        "    fragPos = vec3(model * vec4(aPos, 1.0));\n"
        "    fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "    fragNormal = vec3(normalMatrix * vec4(aNormal, 1.0));\n"
        "    texCoord = aTexCoord;\n"
        "}";

    const char* mainFS =
        "#version 330 core\n"
        "in vec3 fragPos;\n"
        "in vec3 fragNormal;\n"
        "in vec2 texCoord;\n"
        "in vec4 fragPosLightSpace;\n"
        "out vec4 FragColor;\n"
        "uniform int useTexture;\n"
        "uniform sampler2D texture1;\n"
        "uniform sampler2D shadowMap;\n"
        "uniform vec3 materialDiffuseColor;\n"
        "uniform float materialTransparency;\n"
        "uniform float materialRefraction;\n"
        "uniform float materialSpecularIntensity;\n"
        "uniform vec3 materialSpecularColor;\n"
        "uniform vec3 viewPos;\n"
        "uniform vec3 ambientLight;\n"
        "uniform float specularStrength;\n"
        "uniform int shininess;\n"
        "struct DirectionalLight { vec3 direction; vec3 color; float intensity; };\n"
        "uniform DirectionalLight dirLight;\n"
        "struct PointLight { vec3 position; vec3 color; float intensity; float constant; float linear; float quadratic; };\n"
        "#define MAX_POINT_LIGHTS 4\n"
        "uniform PointLight pointLights[MAX_POINT_LIGHTS];\n"
        "uniform int pointLightCount;\n"
        "float calculateShadow(vec4 fPLS, vec3 n, vec3 ld) {\n"
        "    vec3 pc = fPLS.xyz / fPLS.w;\n"
        "    pc = pc * 0.5 + 0.5;\n"
        "    if (pc.z > 1.0) return 0.0;\n"
        "    float bias = max(0.05 * (1.0 - dot(n, ld)), 0.01);\n"
        "    float sh = 0.0;\n"
        "    vec2 ts = 1.0 / textureSize(shadowMap, 0);\n"
        "    for(int x = -1; x <= 1; x++) {\n"
        "        for(int y = -1; y <= 1; y++) {\n"
        "            float pd = texture(shadowMap, pc.xy + vec2(x, y) * ts).r;\n"
        "            sh += (pc.z - bias) > pd ? 1.0 : 0.0;\n"
        "        }\n"
        "    }\n"
        "    return sh / 9.0;\n"
        "}\n"
        "vec3 calcDirLight(DirectionalLight l, vec3 n, vec3 fp, vec3 vd) {\n"
        "    vec3 ld = normalize(-l.direction);\n"
        "    float df = max(dot(n, ld), 0.0);\n"
        "    vec3 dif = df * l.color * l.intensity;\n"
        "    vec3 rd = reflect(-ld, n);\n"
        "    float sp = pow(max(dot(vd, rd), 0.0), shininess);\n"
        "    vec3 spc = materialSpecularIntensity * sp * materialSpecularColor;\n"
        "    float sh = calculateShadow(fragPosLightSpace, n, ld);\n"
        "    return (dif + spc) * (1.0 - sh);\n"
        "}\n"
        "vec3 calcPointLight(PointLight l, vec3 n, vec3 fp, vec3 vd) {\n"
        "    vec3 ld = normalize(l.position - fp);\n"
        "    float df = max(dot(n, ld), 0.0);\n"
        "    vec3 dif = df * l.color * l.intensity;\n"
        "    vec3 rd = reflect(-ld, n);\n"
        "    float sp = pow(max(dot(vd, rd), 0.0), shininess);\n"
        "    vec3 spc = materialSpecularIntensity * sp * materialSpecularColor;\n"
        "    float d = length(l.position - fp);\n"
        "    float at = 1.0 / (l.constant + l.linear * d + l.quadratic * d * d);\n"
        "    return (dif + spc) * at;\n"
        "}\n"
        "void main() {\n"
        "    vec3 n = normalize(fragNormal);\n"
        "    vec3 vd = normalize(viewPos - fragPos);\n"
        "    vec3 res = ambientLight;\n"
        "    res += calcDirLight(dirLight, n, fragPos, vd);\n"
        "    for(int i = 0; i < pointLightCount; i++) res += calcPointLight(pointLights[i], n, fragPos, vd);\n"
        "    vec3 fc = useTexture == 1 ? texture(texture1, texCoord).rgb : materialDiffuseColor;\n"
        "    vec3 flc = clamp(fc * res, vec3(0.0), vec3(1.0));\n"
        "    FragColor = vec4(flc, materialTransparency);\n"
        "}";
    if (!shader.create(mainVS, mainFS)) {
        glfwTerminate();
        return -1;
    }

    // 9. Light Manager (same as myroomfinal.cpp)
    LightManager lightManager;
    lightManager.setAmbientLight(glm::vec3(0.65f));
    DirectionalLight dirLight;
    dirLight.direction = glm::vec3(0.05f, -0.009f, -1.0f);
    dirLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
    dirLight.intensity = 0.25f;
    lightManager.setDirectionalLight(dirLight);
    lightManager.addPointLight({ glm::vec3(29.0f, 20.0f, 14.0f), glm::vec3(1.0f), 2.5f, 1.0f, 0.07f, 0.02f });

    // 10. Initialize Physics
    PhysicsWorld physicsWorld;
    g_physicsWorld = &physicsWorld;
    if (!physicsWorld.init()) {
        glfwTerminate();
        return -1;
    }

    // 11. Load ALL Room Models (from myroomfinal.cpp)
    std::vector<std::unique_ptr<UnifiedModel>> roomModels;

    auto loadRoomModel = [&roomModels](const char* fbx, const char* tex, float scale = 0.1f, glm::vec3 trans = glm::vec3(0)) {
        auto model = std::make_unique<UnifiedModel>();
        if (tex && model->load(fbx, tex)) {
            model->scale(glm::vec3(scale));
            if (trans != glm::vec3(0)) model->translate(trans);
            roomModels.push_back(std::move(model));
        }
        else if (!tex && model->load(fbx)) {
            model->scale(glm::vec3(scale));
            if (trans != glm::vec3(0)) model->translate(trans);
            roomModels.push_back(std::move(model));
        }
        };

    // Load all models from myroomfinal.cpp
    loadRoomModel("res/model/wall1.fbx", "res/texture/light_green.png", 0.1f, glm::vec3(0, 0, -5));
    loadRoomModel("res/model/wall2.fbx", "res/texture/light_green.png", 0.1f, glm::vec3(0, 0, -5));
    loadRoomModel("res/model/wall3.fbx", "res/texture/light_green.png", 0.1f);
    loadRoomModel("res/model/top.fbx", "res/texture/light_green.png", 0.1f);
    loadRoomModel("res/model/floor.fbx", "res/texture/floor.png", 0.1f, glm::vec3(0, -1, 0));
    loadRoomModel("res/model/bed.fbx", "res/texture/bed.png", 0.1f);
    loadRoomModel("res/model/carpet.fbx", "res/texture/carpet.png", 0.1f);
    loadRoomModel("res/model/table.fbx", "res/texture/table.png", 0.1f);
    loadRoomModel("res/model/side_table.fbx", "res/texture/side_table.png", 0.1f);
    loadRoomModel("res/model/littletable.fbx", "res/texture/littletable.png", 0.1f);
    loadRoomModel("res/model/abcposter.fbx", "res/texture/abcposter.png", 0.1f);
    loadRoomModel("res/model/poster1.fbx", "res/texture/poster1.png", 0.1f);
    loadRoomModel("res/model/poster2.fbx", "res/texture/poster2.png", 0.1f);
    loadRoomModel("res/model/poster3.fbx", "res/texture/poster3.png", 0.1f);
    loadRoomModel("res/model/poster4.fbx", "res/texture/poster4.png", 0.1f);
    loadRoomModel("res/model/block.fbx", nullptr, 0.1f);
    loadRoomModel("res/model/plushie.fbx", nullptr, 0.1f);
    loadRoomModel("res/model/window.fbx", nullptr, 0.1f);
    loadRoomModel("res/model/bookcase.fbx", nullptr, 0.1f);
    loadRoomModel("res/model/sofa.fbx", nullptr, 0.1f);
    loadRoomModel("res/model/light.fbx", nullptr, 0.1f);

    std::cout << "Loaded " << roomModels.size() << " room models" << std::endl;

    // 12. Load Block Models 
    std::vector<std::unique_ptr<UnifiedModel>> blockModels(6);
    std::vector<UnifiedModel*> blockModelPtrs(6, nullptr);
    g_blockModelPtrs = &blockModelPtrs;

    std::vector<std::string> objFiles = { OBJ_FILE_1, OBJ_FILE_2, OBJ_FILE_3, OBJ_FILE_4, OBJ_FILE_5, OBJ_FILE_6 };  //      OBJ_FILE_6
    for (int i = 0; i < 6; i++) {  //
        blockModels[i] = std::make_unique<UnifiedModel>();
        // Use loadBlock to ignore original position and center to origin
        if (blockModels[i]->loadBlock(objFiles[i])) {
            blockModelPtrs[i] = blockModels[i].get();
            std::cout << "Loaded block: " << objFiles[i] << std::endl;
        }
    }

    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "Space/Ctrl: Up/Down" << std::endl;
    std::cout << "Right mouse: Rotate view" << std::endl;
    std::cout << "1-6: Generate block (type 1-6)" << std::endl;
    std::cout << "Block Icons: Click toolbar icons to generate blocks" << std::endl;
    std::cout << "Left click: Select block (YELLOW) / Release block" << std::endl;
    std::cout << "Mouse drag: Move block horizontally (when selected)" << std::endl;
    std::cout << "T: Move selected block UP" << std::endl;
    std::cout << "B: Move selected block DOWN" << std::endl;
    std::cout << "R: Reset selected block rotation (fix fallen blocks)" << std::endl;
    std::cout << "================\n" << std::endl;

    // 13. Main Loop
    float lastFrame = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        camera.deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        camera.processInput(window);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update following position (from original updateScene)
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix();
        glm::vec3 camPos = camera.getPosition();
        physicsWorld.updateFollowing(view, projection, camPos);

        physicsWorld.update(camera.deltaTime);

        // Shadow Pass (same as myroomfinal.cpp)
        glm::vec3 lightDir = lightManager.getDirectionalLight().direction;
        float orthoSize = 300.0f;
        glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 250.0f);
        glm::vec3 lightPos = -lightDir * 80.0f;
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowFBO.bind();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClear(GL_DEPTH_BUFFER_BIT);
        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glDisable(GL_CULL_FACE);

        // Draw blocks for shadow 
        for (const auto& geo : physicsWorld.geometries) {
            if (geo.modelIndex >= 0 && geo.modelIndex < 6 && blockModelPtrs[geo.modelIndex]) {  //   Ϊ6
                btTransform trans;
                geo.body->getMotionState()->getWorldTransform(trans);
                btScalar m[16];
                trans.getOpenGLMatrix(m);
                glm::mat4 modelMat = glm::make_mat4(m);
                blockModelPtrs[geo.modelIndex]->drawDepthWithMatrix(depthShader, modelMat);
            }
        }

        // Draw room models for shadow
        for (const auto& model : roomModels) {
            model->drawDepth(depthShader);
        }

        glEnable(GL_CULL_FACE);
        shadowFBO.unbind();

        // Main Render Pass
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(245.0f / 255.0f, 245.0f / 255.0f, 240.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setMat4("view", camera.getViewMatrix());
        shader.setMat4("projection", camera.getProjectionMatrix());
        shader.setVec3("viewPos", camera.getPosition());
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shadowMapTexture.bind(1);
        shader.setInt("shadowMap", 1);
        lightManager.sendToShader(shader);

        // Draw room
        for (const auto& model : roomModels) {
            model->draw(shader);
        }

        // Draw blocks with proper colors (YELLOW when selected - from original)
        for (const auto& geo : physicsWorld.geometries) {
            if (geo.modelIndex >= 0 && geo.modelIndex < 6 && blockModelPtrs[geo.modelIndex]) {  //   Ϊ6
                btTransform trans;
                geo.body->getMotionState()->getWorldTransform(trans);
                btScalar m[16];
                trans.getOpenGLMatrix(m);
                glm::mat4 modelMat = glm::make_mat4(m);

                // YELLOW highlight when selected (from original drawGeometry)
                glm::vec3 drawColor = geo.color;
                bool isSelected = (geo.body == physicsWorld.selectedGeo);
                if (isSelected) {
                    drawColor = glm::vec3(1.0f, 1.0f, 0.0f); // Bright YELLOW
                }

                blockModelPtrs[geo.modelIndex]->drawWithMatrix(shader, modelMat, drawColor, true);
            }
        }

        // Render UI
        renderUI();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    for (auto& model : roomModels) model->cleanup();
    for (auto& model : blockModels) if (model) model->cleanup();
    physicsWorld.cleanup();
    shader.cleanup();
    depthShader.cleanup();
    shadowFBO.cleanup();
    shadowMapTexture.cleanup();

    // Cleanup UI textures
    cleanupBlockIconTextures();

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif