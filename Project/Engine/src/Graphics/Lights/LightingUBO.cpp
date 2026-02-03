#include "pch.h"
#include "Graphics/Lights/LightingUBO.hpp"
#include <iostream>

bool LightingUBO::Initialize()
{
    if (initialized)
    {
        return true;
    }

    glGenBuffers(1, &uboID);
    glBindBuffer(GL_UNIFORM_BUFFER, uboID);

    // Allocate buffer with size of our data struct
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightingDataUBO), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    initialized = true;
    std::cout << "[LightingUBO] Initialized - Buffer ID: " << uboID
        << ", Size: " << sizeof(LightingDataUBO) << " bytes" << std::endl;

    return true;
}

void LightingUBO::Shutdown()
{
    if (uboID != 0)
    {
        glDeleteBuffers(1, &uboID);
        uboID = 0;
    }
    initialized = false;
}

void LightingUBO::Update(const LightingDataUBO& data)
{
    if (!initialized)
    {
        return;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, uboID);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightingDataUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void LightingUBO::Bind(GLuint bindingPoint)
{
    if (!initialized)
    {
        return;
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, uboID);
}