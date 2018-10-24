#include "renderscene.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <ngl/Obj.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>

RenderScene::RenderScene() : m_width(1),
                             m_height(1),
                             m_ratio(1.0f)
{
  std::srand(std::time(nullptr));
  for (int i = 0; i < 100; i++)
  {
    m_randPos[i] = glm::vec3(std::rand()/float(RAND_MAX) * 20.f, std::rand()/float(RAND_MAX) * 20.f, std::rand()/float(RAND_MAX) * 20.f);
    for (int j = 0; j < 3; j++)
    {
      if (std::rand()/float(RAND_MAX) > 0.5f)
      {
        m_randPos[i].operator[](j) *= -1;
      }
    }
  }
}

void RenderScene::resizeGL(GLint _width, GLint _height) noexcept
{
  m_width = _width;
  m_height = _height;
  m_ratio = m_width / float(m_height);
  m_isFBODirty = true;
}

void RenderScene::initGL() noexcept
{
  ngl::NGLInit::instance();
  glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);

  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->loadShader("ColourProgram",
                     "shaders/colour_v.glsl",
                     "shaders/colour_f.glsl");
}

void RenderScene::paintGL() noexcept
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0,0,m_width,m_height);
  ngl::VAOPrimitives* prim = ngl::VAOPrimitives::instance();

  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->use("ColourProgram");
  GLuint pid = shader->getProgramID("ColourProgram");

  for (int i = 0; i < 100; i++)
  {
    //std::cout<<i<<'\n';
    glm::mat4 M;
    M = glm::translate(M, m_randPos[i]);

    glm::mat4 MV = m_view * M;
    glm::mat4 MVP = m_proj * MV;

    glUniformMatrix4fv(glGetUniformLocation(pid, "MV"),
                       1,
                       false,
                       glm::value_ptr(MV));


    glUniformMatrix4fv(glGetUniformLocation(pid, "MVP"),
                       1,
                       false,
                       glm::value_ptr(MVP));

    prim->draw("teapot");
  }
}

void RenderScene::setViewMatrix(glm::mat4 _view)
{
  m_view = _view;
}

void RenderScene::setProjMatrix(glm::mat4 _proj)
{
  m_proj = _proj;
}
