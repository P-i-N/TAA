#include "renderscene.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <ngl/Obj.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>

RenderScene::RenderScene() : m_width(1),
                             m_height(1),
                             m_ratio(1.0f)
{}

RenderScene::~RenderScene() = default;

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
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);

  m_arrObj[0].m_mesh = new ngl::Obj("models/scene2.obj");

  for (auto &i : m_arrObj)
  {
    i.m_mesh->createVAO();
  }

  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  shader->loadShader("environmentShader",
                     "shaders/env_v.glsl",
                     "shaders/env_f.glsl");


  shader->loadShader("phongShader",
                     "shaders/phong_v.glsl",
                     "shaders/phong_f.glsl");

  shader->loadShader("aaShader",
                     "shaders/aa_v.glsl",
                     "shaders/aa_f.glsl");

  shader->loadShader("blitShader",
                     "shaders/blit_v.glsl",
                     "shaders/blit_f.glsl");

  initEnvironment();

  ngl::VAOPrimitives *prim = ngl::VAOPrimitives::instance();
  prim->createTrianglePlane("plane",2,2,1,1,ngl::Vec3(0,1,0));
}

void RenderScene::paintGL() noexcept
{
  //Common stuff
  if (m_isFBODirty)
  {
    initFBO(m_renderFBO, m_renderFBOColour, m_renderFBODepth);
    initFBO(m_aaFBO1, m_aaFBOColour1, m_aaFBODepth1);
    initFBO(m_aaFBO2, m_aaFBOColour2, m_aaFBODepth2);
    m_aaDirty = true;
    m_isFBODirty = false;
  }

  size_t activeAAFBO;
  if (m_flip) {activeAAFBO = m_aaFBO1;}
  else        {activeAAFBO = m_aaFBO2;}

  //Jitter VP matrix (not sure how this affects the MV matrix in phong shader?)
  m_lastVP = m_VP;
  m_VP = m_proj * m_view;
  m_VP = glm::translate(m_VP, m_sampleVector[m_jitterCounter]);

  //Scene
  renderScene(true, activeAAFBO);

  //AA
  if (!m_aaDirty) {antialias(activeAAFBO);}

  //Blit
  if (m_flip) {blit(m_aaFBO1, m_aaFBOColour1, m_aaColourTU1);}
  else        {blit(m_aaFBO2, m_aaFBOColour2, m_aaColourTU2);}

  //Cycle jitter
  m_jitterCounter++;
  if (m_jitterCounter > 3) {m_jitterCounter = 0;}

  m_aaDirty = false;
  m_flip = !m_flip;
}

void RenderScene::antialias(size_t _activeAAFBO)
{
  glBindFramebuffer(GL_FRAMEBUFFER, m_arrFBO[_activeAAFBO][taa_fboID]);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0,0,m_width,m_height);

  ngl::ShaderLib* shader = ngl::ShaderLib::instance();
  ngl::VAOPrimitives* prim = ngl::VAOPrimitives::instance();
  glm::mat4 screenMVP = glm::rotate(glm::mat4(1.0f), glm::pi<float>() * 0.5f, glm::vec3(1.0f,0.0f,0.0f));

  shader->use("aaShader");
  GLuint shaderID = shader->getProgramID("aaShader");

  glm::mat4 inverseVPC = glm::inverse(m_VP);

  glActiveTexture(m_renderFBOColour);
  glBindTexture(GL_TEXTURE_2D, m_arrFBO[m_renderFBO][taa_fboTextureID]);
  glActiveTexture(m_renderFBODepth);
  glBindTexture(GL_TEXTURE_2D, m_arrFBO[m_renderFBO][taa_fboDepthID]);
  //Bind the inactive aaFBO
  if (_activeAAFBO == m_aaFBO1)
  {
    glActiveTexture(m_aaFBOColour2);
    glBindTexture(GL_TEXTURE_2D, m_arrFBO[m_aaFBO2][taa_fboTextureID]);
  }
  else
  {
    glActiveTexture(m_aaFBOColour1);
    glBindTexture(GL_TEXTURE_2D, m_arrFBO[m_aaFBO1][taa_fboTextureID]);
  }

  glUniform1i(glGetUniformLocation(shaderID, "colourRENDER"),       m_renderColourTU);
  glUniform1i(glGetUniformLocation(shaderID, "depthRENDER"),        m_renderDepthTU);
  if (_activeAAFBO == m_aaFBO1) {glUniform1i(glGetUniformLocation(shaderID, "colourANTIALIASED"),  m_aaColourTU2);} //Bind the inactive aaFBO
  else                          {glUniform1i(glGetUniformLocation(shaderID, "colourANTIALIASED"),  m_aaColourTU1);}
  glUniform2f(glGetUniformLocation(shaderID, "windowSize"),         m_width, m_height);
  glUniformMatrix4fv(glGetUniformLocation(shaderID, "inverseViewProjectionCURRENT"),
                     1,
                     false,
                     glm::value_ptr(inverseVPC));
  glUniformMatrix4fv(glGetUniformLocation(shaderID, "viewProjectionHISTORY"),
                     1,
                     false,
                     glm::value_ptr(m_lastVP));
  glUniformMatrix4fv(glGetUniformLocation(shaderID, "MVP"),
                     1,
                     false,
                     glm::value_ptr(screenMVP));
  glUniform3fv(glGetUniformLocation(shaderID, "jitter"),
               1,
               glm::value_ptr(m_sampleVector[m_jitterCounter]));

  prim->draw("plane");
}

void RenderScene::blit(size_t _fbo, GLenum _texture, int _textureUnit)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0,0,m_width,m_height);

  ngl::ShaderLib* shader = ngl::ShaderLib::instance();
  ngl::VAOPrimitives* prim = ngl::VAOPrimitives::instance();
  glm::mat4 screenMVP = glm::rotate(glm::mat4(1.0f), glm::pi<float>() * 0.5f, glm::vec3(1.0f,0.0f,0.0f));

  shader->use("blitShader");
  GLuint shaderID = shader->getProgramID("blitShader");

  glActiveTexture(_texture);
  glBindTexture(GL_TEXTURE_2D, m_arrFBO[_fbo][taa_fboTextureID]);

  glUniform1i(glGetUniformLocation(shaderID, "inputTex"), _textureUnit);
  glUniform2f(glGetUniformLocation(shaderID, "windowSize"), m_width, m_height);
  glUniformMatrix4fv(glGetUniformLocation(shaderID, "MVP"),
                     1,
                     false,
                     glm::value_ptr(screenMVP));

  prim->draw("plane");
  //glBindTexture(GL_TEXTURE_2D, 0); //not sure why this is here
}

void RenderScene::renderCubemap()
{
  glm::mat4 cubeM, cubeMV, cubeMVP;
  glm::mat3 cubeN;

  ngl::ShaderLib* shader = ngl::ShaderLib::instance();
  GLuint pid = shader->getProgramID("environmentShader");
  shader->use("environmentShader");

  cubeM = glm::mat4(1.f);
  cubeM = glm::scale(cubeM, glm::vec3(200.f, 200.f, 200.f));
  cubeMV = m_cube * cubeM;
  cubeMVP = m_proj * cubeMV;
  cubeN = glm::inverse(glm::mat3(cubeMV));

  glUniformMatrix4fv(glGetUniformLocation(pid, "MVP"),
                     1,
                     false,
                     glm::value_ptr(cubeMVP));
  glUniformMatrix4fv(glGetUniformLocation(pid, "MV"),
                     1,
                     false,
                     glm::value_ptr(cubeMV));

  ngl::VAOPrimitives* prim = ngl::VAOPrimitives::instance();
  prim->draw("cube");
}

void RenderScene::renderScene(bool _cubemap, size_t _activeAAFBO)
{
  if (m_aaDirty) {glBindFramebuffer(GL_FRAMEBUFFER, m_arrFBO[_activeAAFBO][taa_fboID]);}
  else           {glBindFramebuffer(GL_FRAMEBUFFER, m_arrFBO[m_renderFBO][taa_fboID]);}
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0,0,m_width,m_height);
  ngl::ShaderLib* shader = ngl::ShaderLib::instance();
  GLuint pid = shader->getProgramID("phongShader");
  shader->use("phongShader");

  glm::mat4 M, MV, MVP;
  glm::mat3 N;
  M = glm::mat4(1.f);
  M = glm::rotate(M, glm::pi<float>() * 0.25f, {0.f, 1.f, 0.f});
  MV = m_view * M;
  MVP = m_VP * M;
  N = glm::inverse(glm::mat3(MV));

  glUniformMatrix4fv(glGetUniformLocation(pid, "MV"),
                     1,
                     false,
                     glm::value_ptr(MV));
  glUniformMatrix4fv(glGetUniformLocation(pid, "MVP"),
                     1,
                     false,
                     glm::value_ptr(MVP));
  glUniformMatrix3fv(glGetUniformLocation(pid, "N"),
                     1,
                     true,
                     glm::value_ptr(N));

  for (auto &obj : m_arrObj)
  {
    obj.m_mesh->draw();
  }
  if (_cubemap) {renderCubemap();}
}

void RenderScene::setViewMatrix(glm::mat4 _view)
{
  m_lastView = m_view;
  m_view = _view;
}

void RenderScene::setProjMatrix(glm::mat4 _proj)
{
  m_lastProj = m_proj;
  m_proj = _proj;
}

void RenderScene::setCubeMatrix(glm::mat4 _cube)
{
  m_cube = _cube;
}

void RenderScene::initEnvironment()
{
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &m_envTex);
  glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTex);

  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "images/nz.png");
  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "images/pz.png");
  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "images/ny.png");
  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "images/py.png");
  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "images/nx.png");
  initEnvironmentSide(GL_TEXTURE_CUBE_MAP_POSITIVE_X, "images/px.png");

  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_AUTO_GENERATE_MIPMAP, GL_TRUE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GLfloat anisotropy;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &anisotropy);
  glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);

  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->use("environmentShader");
  shader->setUniform("envMap", 0);
}

void RenderScene::initEnvironmentSide(GLenum _target, const char *_filename)
{
    ngl::Image img(_filename);
    glTexImage2D(_target,
                 0,
                 int(img.format()),
                 int(img.width()),
                 int(img.height()),
                 0,
                 img.format(),
                 GL_UNSIGNED_BYTE,
                 img.getPixels());
}


void RenderScene::initFBO(size_t _fboID, GLenum _textureA, GLenum _textureB)
{
  glBindFramebuffer(GL_FRAMEBUFFER, m_arrFBO[_fboID][taa_fboID]);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == (GL_FRAMEBUFFER_COMPLETE))
  {
   glDeleteTextures(1, &m_arrFBO[_fboID][taa_fboTextureID]);
   glDeleteTextures(1, &m_arrFBO[_fboID][taa_fboDepthID]);
   glDeleteFramebuffers(1, &m_arrFBO[_fboID][taa_fboID]);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glGenTextures(1, &m_arrFBO[_fboID][taa_fboTextureID]);
  glActiveTexture(_textureA);
  glBindTexture(GL_TEXTURE_2D, m_arrFBO[_fboID][taa_fboTextureID]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glGenTextures(1, &m_arrFBO[_fboID][taa_fboDepthID]);
  glActiveTexture(_textureB);
  glBindTexture(GL_TEXTURE_2D, m_arrFBO[_fboID][taa_fboDepthID]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glGenFramebuffers(1, &m_arrFBO[_fboID][taa_fboID]);
  glBindFramebuffer(GL_FRAMEBUFFER, m_arrFBO[_fboID][taa_fboID]);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_arrFBO[_fboID][taa_fboTextureID], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_arrFBO[_fboID][taa_fboDepthID], 0);

  GLenum drawBufs[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, drawBufs);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {std::cout<<"Help\n";}
}
