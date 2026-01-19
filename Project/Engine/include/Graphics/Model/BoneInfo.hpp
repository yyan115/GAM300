#pragma once

struct BoneInfo
{
	// Id is index in finalBoneMatrices
	int id;

	// Offset matrix transforms vertex from model space to bone space
	glm::mat4 offset;
};