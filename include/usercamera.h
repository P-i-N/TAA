#ifndef USERCAMERA_H
#define USERCAMERA_H

#include <ngl/Obj.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class UserCamera
{
  public:
    UserCamera();
    void handleMouseMove(const double _xpos, const double _ypos);
    void handleMouseClick(const double _xpos, const double _ypos, const int _button, const int _action, const int _mods);
    void handleKey(const int _key, const bool _state);
    void resize(const int _width, const int _height);
    void update();
    glm::mat4 viewMatrix() const;
    glm::mat4 projMatrix() const;


  private:
    glm::vec3 m_position;
    glm::vec3 m_rotation;
    glm::vec3 m_velocity;
    glm::vec3 m_accelleration;
    glm::vec3 m_target;
    int m_width;
    int m_height;
    glm::mat4 m_view;
    glm::mat4 m_proj;
    float m_fovy;
    float m_aspect;
    float m_zNear;
    float m_zFar;
};

#endif