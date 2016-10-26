#include "LoadMD5.h"


LoadMD5::LoadMD5()
{
	OBJname = L"Object";

	PositionValue = { 0.0f, 0.0f, 0.0f };
	RotationValue = { 0.0f, 0.0f, 0.0f };
	ScaleValue = { 1.0f, 1.0f, 1.0f };

	XMStoreFloat4x4(&Rotation, XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	XMStoreFloat4x4(&Scale, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&Translation, XMMatrixTranslation(0.0f, 0.0f, 0.0f));

	MeshMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	MeshMat.Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	MeshMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);

	FocusMeshMat.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	FocusMeshMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	FocusMeshMat.Specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 16.0f);
}
LoadMD5::~LoadMD5()
{
}
void LoadMD5::Setup(ID3D11Device* d3d11device, ID3D11DeviceContext* d3d11devCon, IDXGISwapChain* swapchain)
{
	d3d11Device = d3d11device;
	d3d11DevCon = d3d11devCon;
	SwapChain = swapchain;
}
void LoadMD5::Init(ID3D11SamplerState* texsamplerstate,
	ID3D11RasterizerState* rscullnone)
{
	TexSamplerState = texsamplerstate;
	RSCullNone = rscullnone;
}
void LoadMD5::CleanUp()
{
	for (int i = 0; i < MD5Model.numSubsets; i++)
	{
		MD5Model.subsets[i].indexBuff->Release();
		MD5Model.subsets[i].vertBuff->Release();
	}
}
void LoadMD5::Update(const XMFLOAT4X4& camview, const XMFLOAT4X4& camprojection, const float& tick)
{
	XMMATRIX ro = XMLoadFloat4x4(&Rotation);
	XMMATRIX scale = XMLoadFloat4x4(&Scale);
	XMMATRIX translation = XMLoadFloat4x4(&Translation);

	XMMATRIX meshworld = ro * scale * translation;
	XMStoreFloat4x4(&MeshWorld, meshworld);

	camView = camview;
	camProjection = camprojection;

	if (MD5Model.animations.size() > 0 && AniPlay)
		UpdateMD5Model(tick * AniSpeed, PlayAniNum);
}
void LoadMD5::DrawSceneToShadowMap(const XMFLOAT4X4& mLightView, const XMFLOAT4X4& mLightProj, const XMVECTOR& camerapos)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMFLOAT4 campos4;
	XMStoreFloat4(&campos4, camerapos);
	XMFLOAT3 campos = { campos4.x, campos4.y, campos4.z };

	Effects::BuildShadowMapFX->SetEyePosW(campos);
	Effects::BuildShadowMapFX->SetViewProj(viewProj);

	// These properties could be set per object if needed.
	Effects::BuildShadowMapFX->SetHeightScale(0.07f);
	Effects::BuildShadowMapFX->SetMaxTessDistance(1.0f);
	Effects::BuildShadowMapFX->SetMinTessDistance(25.0f);
	Effects::BuildShadowMapFX->SetMinTessFactor(1.0f);
	Effects::BuildShadowMapFX->SetMaxTessFactor(5.0f);

	ID3DX11EffectTechnique* tessSmapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;
	ID3DX11EffectTechnique* smapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;

	d3d11DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	smapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;
	tessSmapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;

	XMMATRIX meshworld = XMLoadFloat4x4(&MeshWorld);
	XMMATRIX WVP;

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	D3DX11_TECHNIQUE_DESC techDesc;
	tessSmapTech->GetDesc(&techDesc);

	d3d11DevCon->HSSetShader(0, 0, 0);
	d3d11DevCon->DSSetShader(0, 0, 0);

	smapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;
	smapTech->GetDesc(&techDesc);

	d3d11DevCon->IASetInputLayout(InputLayouts::PosNormal);

	for (int i = 0; i < MD5Model.numSubsets; i++)
	{
		//Set the grounds index buffer
		d3d11DevCon->IASetIndexBuffer(MD5Model.subsets[i].indexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		d3d11DevCon->IASetVertexBuffers(0, 1, &MD5Model.subsets[i].vertBuff, &stride, &offset);

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = meshworld * view * proj;

		Effects::BuildShadowMapFX->SetWorld(meshworld);
		Effects::BuildShadowMapFX->SetWorldInvTranspose(XMMatrixTranspose(meshworld));
		Effects::BuildShadowMapFX->SetWorldViewProj(WVP);
		Effects::BuildShadowMapFX->SetTexTransform(XMMatrixIdentity());

		//d3d11DevCon->PSSetSamplers(0, 1, &TexSamplerState);

		smapTech->GetPassByIndex(i)->Apply(0, d3d11DevCon);
		d3d11DevCon->DrawIndexed(MD5Model.subsets[i].indices.size(), 0, 0);
	}
}
void LoadMD5::OnRender(const XMFLOAT4X4& shadowTransform)
{
	//Set Primitive Topology
	d3d11DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	XMMATRIX meshworld = XMLoadFloat4x4(&MeshWorld);
	XMMATRIX WVP;

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	ID3DX11EffectTechnique* activeTech = Effects::BasicFX->Light1Tech;

	XMMATRIX mshadowTransform = XMLoadFloat4x4(&shadowTransform);

	D3DX11_TECHNIQUE_DESC techDesc;
	activeTech->GetDesc(&techDesc);

	///////////////**************new**************////////////////////
	///***Draw MD5 Model***///
	for (int i = 0; i < MD5Model.numSubsets; i++)
	{
		//Set the grounds index buffer
		d3d11DevCon->IASetIndexBuffer(MD5Model.subsets[i].indexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		d3d11DevCon->IASetVertexBuffers(0, 1, &MD5Model.subsets[i].vertBuff, &stride, &offset);

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = meshworld * XMLoadFloat4x4(&camView) * XMLoadFloat4x4(&camProjection);

		Effects::BasicFX->SetWorld(meshworld);
		Effects::BasicFX->SetWorldInvTranspose(XMMatrixTranspose(meshworld));
		Effects::BasicFX->SetWorldViewProj(WVP);
		Effects::BasicFX->SetShadowTransform(meshworld * mshadowTransform);
		Effects::BasicFX->SetTexTransform(XMMatrixScaling(1.0f, 1.0f, 1.0f));
		if (FocusSetting)
			Effects::BasicFX->SetMaterial(FocusMeshMat);
		else
			Effects::BasicFX->SetMaterial(MeshMat);

		Effects::BasicFX->SethasTexture(true);
		Effects::BasicFX->SethasNormal(false);		

		Effects::BasicFX->SetDiffuseMap(meshSRV[MD5Model.subsets[i].texArrayIndex]);

		//d3d11DevCon->PSSetSamplers(0, 1, &TexSamplerState);

		d3d11DevCon->RSSetState(RSCullNone);
		activeTech->GetPassByIndex(i)->Apply(0, d3d11DevCon);
		d3d11DevCon->DrawIndexed(MD5Model.subsets[i].indices.size(), 0, 0);
	}
	
	///////////////**************new**************////////////////////
}

bool LoadMD5::LoadMD5Model(std::wstring filename)
{
	HRESULT hr;

	std::wifstream fileIn(filename.c_str());		// Open file

	std::wstring checkString;						// Stores the next string from our file

	if (fileIn)										// Check if the file was opened
	{
		while (fileIn)								// Loop until the end of the file is reached
		{
			fileIn >> checkString;					// Get next string from file

			if (checkString == L"MD5Version")		// Get MD5 version (this function supports version 10)
			{
				/*fileIn >> checkString;
				MessageBox(0, checkString.c_str(),	//display message
				L"MD5Version", MB_OK);*/
			}
			else if (checkString == L"commandline")
			{
				std::getline(fileIn, checkString);	// Ignore the rest of this line
			}
			else if (checkString == L"numJoints")
			{
				fileIn >> MD5Model.numJoints;		// Store number of joints
			}
			else if (checkString == L"numMeshes")
			{
				fileIn >> MD5Model.numSubsets;		// Store number of meshes or subsets which we will call them
			}
			else if (checkString == L"joints")
			{
				Joint tempJoint;

				fileIn >> checkString;				// Skip the "{"

				for (int i = 0; i < MD5Model.numJoints; i++)
				{
					fileIn >> tempJoint.name;		// Store joints name
					// Sometimes the names might contain spaces. If that is the case, we need to continue
					// to read the name until we get to the closing " (quotation marks)
					if (tempJoint.name[tempJoint.name.size() - 1] != '"')
					{
						wchar_t checkChar;
						bool jointNameFound = false;
						while (!jointNameFound)
						{
							checkChar = fileIn.get();

							if (checkChar == '"')
								jointNameFound = true;

							tempJoint.name += checkChar;
						}
					}

					fileIn >> tempJoint.parentID;	// Store Parent joint's ID

					fileIn >> checkString;			// Skip the "("

					// Store position of this joint (swap y and z axis if model was made in RH Coord Sys)
					fileIn >> tempJoint.pos.x >> tempJoint.pos.z >> tempJoint.pos.y;

					fileIn >> checkString >> checkString;	// Skip the ")" and "("

					// Store orientation of this joint
					fileIn >> tempJoint.orientation.x >> tempJoint.orientation.z >> tempJoint.orientation.y;

					// Remove the quotation marks from joints name
					tempJoint.name.erase(0, 1);
					tempJoint.name.erase(tempJoint.name.size() - 1, 1);

					// Compute the w axis of the quaternion (The MD5 model uses a 3D vector to describe the
					// direction the bone is facing. However, we need to turn this into a quaternion, and the way
					// quaternions work, is the xyz values describe the axis of rotation, while the w is a value
					// between 0 and 1 which describes the angle of rotation)
					float t = 1.0f - (tempJoint.orientation.x * tempJoint.orientation.x)
						- (tempJoint.orientation.y * tempJoint.orientation.y)
						- (tempJoint.orientation.z * tempJoint.orientation.z);
					if (t < 0.0f)
					{
						tempJoint.orientation.w = 0.0f;
					}
					else
					{
						tempJoint.orientation.w = -sqrtf(t);
					}

					std::getline(fileIn, checkString);		// Skip rest of this line

					MD5Model.joints.push_back(tempJoint);	// Store the joint into this models joint vector
				}

				fileIn >> checkString;					// Skip the "}"
			}
			else if (checkString == L"mesh")
			{
				ModelSubset subset;
				int numVerts, numTris, numWeights;

				fileIn >> checkString;					// Skip the "{"

				fileIn >> checkString;
				while (checkString != L"}")			// Read until '}'
				{
					// In this lesson, for the sake of simplicity, we will assume a textures filename is givin here.
					// Usually though, the name of a material (stored in a material library. Think back to the lesson on
					// loading .obj files, where the material library was contained in the file .mtl) is givin. Let this
					// be an exercise to load the material from a material library such as obj's .mtl file, instead of
					// just the texture like we will do here.
					if (checkString == L"shader")		// Load the texture or material
					{
						std::wstring fileNamePath;
						fileIn >> fileNamePath;			// Get texture's filename

						// Take spaces into account if filename or material name has a space in it
						if (fileNamePath[fileNamePath.size() - 1] != '"')
						{
							wchar_t checkChar;
							bool fileNameFound = false;
							while (!fileNameFound)
							{
								checkChar = fileIn.get();

								if (checkChar == '"')
									fileNameFound = true;

								fileNamePath += checkChar;
							}
						}

						// Remove the quotation marks from texture path
						fileNamePath.erase(0, 1);
						fileNamePath.erase(fileNamePath.size() - 1, 1);

						//check if this texture has already been loaded
						bool alreadyLoaded = false;
						for (int i = 0; i < textureNameArray.size(); ++i)
						{
							if (fileNamePath == textureNameArray[i])
							{
								alreadyLoaded = true;
								subset.texArrayIndex = i;
							}
						}

						//if the texture is not already loaded, load it now
						if (!alreadyLoaded)
						{
							std::wstring pluspath = L".\\model\\";
							pluspath += fileNamePath;
							ID3D11ShaderResourceView* tempMeshSRV;
							hr = D3DX11CreateShaderResourceViewFromFile(d3d11Device, pluspath.c_str(),
								NULL, NULL, &tempMeshSRV, NULL);
							if (SUCCEEDED(hr))
							{
								textureNameArray.push_back(fileNamePath.c_str());
								subset.texArrayIndex = meshSRV.size();
								meshSRV.push_back(tempMeshSRV);
							}
							else
							{
								MessageBox(0, pluspath.c_str(),		//display message
									L"Could Not Open:", MB_OK);
								return false;
							}
						}

						std::getline(fileIn, checkString);				// Skip rest of this line
					}
					else if (checkString == L"numverts")
					{
						fileIn >> numVerts;								// Store number of vertices

						std::getline(fileIn, checkString);				// Skip rest of this line

						for (int i = 0; i < numVerts; i++)
						{
							Vertex tempVert;

							fileIn >> checkString						// Skip "vert # ("
								>> checkString
								>> checkString;

							fileIn >> tempVert.texCoord.x				// Store tex coords
								>> tempVert.texCoord.y;

							fileIn >> checkString;						// Skip ")"

							fileIn >> tempVert.StartWeight;				// Index of first weight this vert will be weighted to

							fileIn >> tempVert.WeightCount;				// Number of weights for this vertex

							std::getline(fileIn, checkString);			// Skip rest of this line

							subset.vertices.push_back(tempVert);		// Push back this vertex into subsets vertex vector
						}
					}
					else if (checkString == L"numtris")
					{
						fileIn >> numTris;
						subset.numTriangles = numTris;

						std::getline(fileIn, checkString);				// Skip rest of this line

						for (int i = 0; i < numTris; i++)				// Loop through each triangle
						{
							DWORD tempIndex;
							fileIn >> checkString;						// Skip "tri"
							fileIn >> checkString;						// Skip tri counter

							for (int k = 0; k < 3; k++)					// Store the 3 indices
							{
								fileIn >> tempIndex;
								subset.indices.push_back(tempIndex);
							}

							std::getline(fileIn, checkString);			// Skip rest of this line
						}
					}
					else if (checkString == L"numweights")
					{
						fileIn >> numWeights;

						std::getline(fileIn, checkString);				// Skip rest of this line

						for (int i = 0; i < numWeights; i++)
						{
							Weight tempWeight;
							fileIn >> checkString >> checkString;		// Skip "weight #"

							fileIn >> tempWeight.jointID;				// Store weight's joint ID

							fileIn >> tempWeight.bias;					// Store weight's influence over a vertex

							fileIn >> checkString;						// Skip "("

							fileIn >> tempWeight.pos.x					// Store weight's pos in joint's local space
								>> tempWeight.pos.z
								>> tempWeight.pos.y;

							std::getline(fileIn, checkString);			// Skip rest of this line

							subset.weights.push_back(tempWeight);		// Push back tempWeight into subsets Weight array
						}

					}
					else
						std::getline(fileIn, checkString);				// Skip anything else

					fileIn >> checkString;								// Skip "}"
				}

				//*** find each vertex's position using the joints and weights ***//
				for (int i = 0; i < subset.vertices.size(); ++i)
				{
					Vertex tempVert = subset.vertices[i];
					tempVert.pos = XMFLOAT3(0, 0, 0);	// Make sure the vertex's pos is cleared first

					// Sum up the joints and weights information to get vertex's position
					for (int j = 0; j < tempVert.WeightCount; ++j)
					{
						Weight tempWeight = subset.weights[tempVert.StartWeight + j];
						Joint tempJoint = MD5Model.joints[tempWeight.jointID];

						// Convert joint orientation and weight pos to vectors for easier computation
						// When converting a 3d vector to a quaternion, you should put 0 for "w", and
						// When converting a quaternion to a 3d vector, you can just ignore the "w"
						XMVECTOR tempJointOrientation = XMVectorSet(tempJoint.orientation.x, tempJoint.orientation.y, tempJoint.orientation.z, tempJoint.orientation.w);
						XMVECTOR tempWeightPos = XMVectorSet(tempWeight.pos.x, tempWeight.pos.y, tempWeight.pos.z, 0.0f);

						// We will need to use the conjugate of the joint orientation quaternion
						// To get the conjugate of a quaternion, all you have to do is inverse the x, y, and z
						XMVECTOR tempJointOrientationConjugate = XMVectorSet(-tempJoint.orientation.x, -tempJoint.orientation.y, -tempJoint.orientation.z, tempJoint.orientation.w);

						// Calculate vertex position (in joint space, eg. rotate the point around (0,0,0)) for this weight using the joint orientation quaternion and its conjugate
						// We can rotate a point using a quaternion with the equation "rotatedPoint = quaternion * point * quaternionConjugate"
						XMFLOAT3 rotatedPoint;
						XMStoreFloat3(&rotatedPoint, XMQuaternionMultiply(XMQuaternionMultiply(tempJointOrientation, tempWeightPos), tempJointOrientationConjugate));

						// Now move the verices position from joint space (0,0,0) to the joints position in world space, taking the weights bias into account
						// The weight bias is used because multiple weights might have an effect on the vertices final position. Each weight is attached to one joint.
						tempVert.pos.x += (tempJoint.pos.x + rotatedPoint.x) * tempWeight.bias;
						tempVert.pos.y += (tempJoint.pos.y + rotatedPoint.y) * tempWeight.bias;
						tempVert.pos.z += (tempJoint.pos.z + rotatedPoint.z) * tempWeight.bias;

						// Basically what has happened above, is we have taken the weights position relative to the joints position
						// we then rotate the weights position (so that the weight is actually being rotated around (0, 0, 0) in world space) using
						// the quaternion describing the joints rotation. We have stored this rotated point in rotatedPoint, which we then add to
						// the joints position (because we rotated the weight's position around (0,0,0) in world space, and now need to translate it
						// so that it appears to have been rotated around the joints position). Finally we multiply the answer with the weights bias,
						// or how much control the weight has over the final vertices position. All weight's bias effecting a single vertex's position
						// must add up to 1.
					}

					subset.positions.push_back(tempVert.pos);			// Store the vertices position in the position vector instead of straight into the vertex vector
					// since we can use the positions vector for certain things like collision detection or picking
					// without having to work with the entire vertex structure.
				}

				// Put the positions into the vertices for this subset
				for (int i = 0; i < subset.vertices.size(); i++)
				{
					subset.vertices[i].pos = subset.positions[i];
				}

				//*** Calculate vertex normals using normal averaging ***///
				std::vector<XMFLOAT3> tempNormal;

				//normalized and unnormalized normals
				XMFLOAT3 unnormalized = XMFLOAT3(0.0f, 0.0f, 0.0f);

				//Used to get vectors (sides) from the position of the verts
				float vecX, vecY, vecZ;

				//Two edges of our triangle
				XMVECTOR edge1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
				XMVECTOR edge2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

				//Compute face normals
				for (int i = 0; i < subset.numTriangles; ++i)
				{
					//Get the vector describing one edge of our triangle (edge 0,2)
					vecX = subset.vertices[subset.indices[(i * 3)]].pos.x - subset.vertices[subset.indices[(i * 3) + 2]].pos.x;
					vecY = subset.vertices[subset.indices[(i * 3)]].pos.y - subset.vertices[subset.indices[(i * 3) + 2]].pos.y;
					vecZ = subset.vertices[subset.indices[(i * 3)]].pos.z - subset.vertices[subset.indices[(i * 3) + 2]].pos.z;
					edge1 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our first edge

					//Get the vector describing another edge of our triangle (edge 2,1)
					vecX = subset.vertices[subset.indices[(i * 3) + 2]].pos.x - subset.vertices[subset.indices[(i * 3) + 1]].pos.x;
					vecY = subset.vertices[subset.indices[(i * 3) + 2]].pos.y - subset.vertices[subset.indices[(i * 3) + 1]].pos.y;
					vecZ = subset.vertices[subset.indices[(i * 3) + 2]].pos.z - subset.vertices[subset.indices[(i * 3) + 1]].pos.z;
					edge2 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our second edge

					//Cross multiply the two edge vectors to get the un-normalized face normal
					XMStoreFloat3(&unnormalized, XMVector3Cross(edge1, edge2));

					tempNormal.push_back(unnormalized);
				}

				//Compute vertex normals (normal Averaging)
				XMVECTOR normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
				int facesUsing = 0;
				float tX, tY, tZ;	//temp axis variables

				//Go through each vertex
				for (int i = 0; i < subset.vertices.size(); ++i)
				{
					//Check which triangles use this vertex
					for (int j = 0; j < subset.numTriangles; ++j)
					{
						if (subset.indices[j * 3] == i ||
							subset.indices[(j * 3) + 1] == i ||
							subset.indices[(j * 3) + 2] == i)
						{
							tX = XMVectorGetX(normalSum) + tempNormal[j].x;
							tY = XMVectorGetY(normalSum) + tempNormal[j].y;
							tZ = XMVectorGetZ(normalSum) + tempNormal[j].z;

							normalSum = XMVectorSet(tX, tY, tZ, 0.0f);	//If a face is using the vertex, add the unormalized face normal to the normalSum

							facesUsing++;
						}
					}

					//Get the actual normal by dividing the normalSum by the number of faces sharing the vertex
					normalSum = normalSum / facesUsing;

					//Normalize the normalSum vector
					normalSum = XMVector3Normalize(normalSum);

					//Store the normal and tangent in our current vertex
					subset.vertices[i].normal.x = -XMVectorGetX(normalSum);
					subset.vertices[i].normal.y = -XMVectorGetY(normalSum);
					subset.vertices[i].normal.z = -XMVectorGetZ(normalSum);

					//Clear normalSum, facesUsing for next vertex
					normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
					facesUsing = 0;
				}

				// Create index buffer
				D3D11_BUFFER_DESC indexBufferDesc;
				ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

				indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
				indexBufferDesc.ByteWidth = sizeof(DWORD)* subset.numTriangles * 3;
				indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
				indexBufferDesc.CPUAccessFlags = 0;
				indexBufferDesc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA iinitData;

				iinitData.pSysMem = &subset.indices[0];
				d3d11Device->CreateBuffer(&indexBufferDesc, &iinitData, &subset.indexBuff);

				//Create Vertex Buffer
				D3D11_BUFFER_DESC vertexBufferDesc;
				ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

				vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;							// We will be updating this buffer, so we must set as dynamic
				vertexBufferDesc.ByteWidth = sizeof(Vertex)* subset.vertices.size();
				vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;				// Give CPU power to write to buffer
				vertexBufferDesc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA vertexBufferData;

				ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
				vertexBufferData.pSysMem = &subset.vertices[0];
				hr = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &subset.vertBuff);

				// Push back the temp subset into the models subset vector
				MD5Model.subsets.push_back(subset);
			}
		}
	}
	else
	{
		SwapChain->SetFullscreenState(false, NULL);	// Make sure we are out of fullscreen

		// create message
		std::wstring message = L"Could not open: ";
		message += filename;

		MessageBox(0, message.c_str(),	// display message
			L"Error", MB_OK);

		return false;
	}

	return true;
}

bool LoadMD5::LoadMD5Anim(std::wstring filename)
{
	ModelAnimation tempAnim;						// Temp animation to later store in our model's animation array

	std::wifstream fileIn(filename.c_str());		// Open file

	std::wstring checkString;						// Stores the next string from our file

	if (fileIn)										// Check if the file was opened
	{
		while (fileIn)								// Loop until the end of the file is reached
		{
			fileIn >> checkString;					// Get next string from file

			if (checkString == L"MD5Version")		// Get MD5 version (this function supports version 10)
			{
				fileIn >> checkString;
				/*MessageBox(0, checkString.c_str(),	//display message
				L"MD5Version", MB_OK);*/
			}
			else if (checkString == L"commandline")
			{
				std::getline(fileIn, checkString);	// Ignore the rest of this line
			}
			else if (checkString == L"numFrames")
			{
				fileIn >> tempAnim.numFrames;				// Store number of frames in this animation
			}
			else if (checkString == L"numJoints")
			{
				fileIn >> tempAnim.numJoints;				// Store number of joints (must match .md5mesh)
			}
			else if (checkString == L"frameRate")
			{
				fileIn >> tempAnim.frameRate;				// Store animation's frame rate (frames per second)
			}
			else if (checkString == L"numAnimatedComponents")
			{
				fileIn >> tempAnim.numAnimatedComponents;	// Number of components in each frame section
			}
			else if (checkString == L"hierarchy")
			{
				fileIn >> checkString;				// Skip opening bracket "{"

				for (int i = 0; i < tempAnim.numJoints; i++)	// Load in each joint
				{
					AnimJointInfo tempJoint;

					fileIn >> tempJoint.name;		// Get joints name
					// Sometimes the names might contain spaces. If that is the case, we need to continue
					// to read the name until we get to the closing " (quotation marks)
					if (tempJoint.name[tempJoint.name.size() - 1] != '"')
					{
						wchar_t checkChar;
						bool jointNameFound = false;
						while (!jointNameFound)
						{
							checkChar = fileIn.get();

							if (checkChar == '"')
								jointNameFound = true;

							tempJoint.name += checkChar;
						}
					}

					// Remove the quotation marks from joints name
					tempJoint.name.erase(0, 1);
					tempJoint.name.erase(tempJoint.name.size() - 1, 1);

					fileIn >> tempJoint.parentID;			// Get joints parent ID
					fileIn >> tempJoint.flags;				// Get flags
					fileIn >> tempJoint.startIndex;			// Get joints start index

					// Make sure the joint exists in the model, and the parent ID's match up
					// because the bind pose (md5mesh) joint hierarchy and the animations (md5anim)
					// joint hierarchy must match up
					bool jointMatchFound = false;
					for (int k = 0; k < MD5Model.numJoints; k++)
					{
						if (MD5Model.joints[k].name == tempJoint.name)
						{
							if (MD5Model.joints[k].parentID == tempJoint.parentID)
							{
								jointMatchFound = true;
								tempAnim.jointInfo.push_back(tempJoint);
							}
						}
					}
					if (!jointMatchFound)					// If the skeleton system does not match up, return false
						return false;						// You might want to add an error message here

					std::getline(fileIn, checkString);		// Skip rest of this line
				}
			}
			else if (checkString == L"bounds")			// Load in the AABB for each animation
			{
				fileIn >> checkString;						// Skip opening bracket "{"

				for (int i = 0; i < tempAnim.numFrames; i++)
				{
					BoundingBox tempBB;

					fileIn >> checkString;					// Skip "("
					fileIn >> tempBB.min.x >> tempBB.min.z >> tempBB.min.y;
					fileIn >> checkString >> checkString;	// Skip ") ("
					fileIn >> tempBB.max.x >> tempBB.max.z >> tempBB.max.y;
					fileIn >> checkString;					// Skip ")"

					tempAnim.frameBounds.push_back(tempBB);
				}
			}
			else if (checkString == L"baseframe")			// This is the default position for the animation
			{												// All frames will build their skeletons off this
				fileIn >> checkString;						// Skip opening bracket "{"

				for (int i = 0; i < tempAnim.numJoints; i++)
				{
					Joint tempBFJ;

					fileIn >> checkString;						// Skip "("
					fileIn >> tempBFJ.pos.x >> tempBFJ.pos.z >> tempBFJ.pos.y;
					fileIn >> checkString >> checkString;		// Skip ") ("
					fileIn >> tempBFJ.orientation.x >> tempBFJ.orientation.z >> tempBFJ.orientation.y;
					fileIn >> checkString;						// Skip ")"

					tempAnim.baseFrameJoints.push_back(tempBFJ);
				}
			}
			else if (checkString == L"frame")		// Load in each frames skeleton (the parts of each joint that changed from the base frame)
			{
				FrameData tempFrame;

				fileIn >> tempFrame.frameID;		// Get the frame ID

				fileIn >> checkString;				// Skip opening bracket "{"

				for (int i = 0; i < tempAnim.numAnimatedComponents; i++)
				{
					float tempData;
					fileIn >> tempData;				// Get the data

					tempFrame.frameData.push_back(tempData);
				}

				tempAnim.frameData.push_back(tempFrame);

				///*** build the frame skeleton ***///
				std::vector<Joint> tempSkeleton;

				for (int i = 0; i < tempAnim.jointInfo.size(); i++)
				{
					int k = 0;						// Keep track of position in frameData array

					// Start the frames joint with the base frame's joint
					Joint tempFrameJoint = tempAnim.baseFrameJoints[i];

					tempFrameJoint.parentID = tempAnim.jointInfo[i].parentID;

					// Notice how I have been flipping y and z. this is because some modeling programs such as
					// 3ds max (which is what I use) use a right handed coordinate system. Because of this, we
					// need to flip the y and z axes. If your having problems loading some models, it's possible
					// the model was created in a left hand coordinate system. in that case, just reflip all the
					// y and z axes in our md5 mesh and anim loader.
					if (tempAnim.jointInfo[i].flags & 1)		// pos.x	( 000001 )
						tempFrameJoint.pos.x = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];

					if (tempAnim.jointInfo[i].flags & 2)		// pos.y	( 000010 )
						tempFrameJoint.pos.z = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];

					if (tempAnim.jointInfo[i].flags & 4)		// pos.z	( 000100 )
						tempFrameJoint.pos.y = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];

					if (tempAnim.jointInfo[i].flags & 8)		// orientation.x	( 001000 )
						tempFrameJoint.orientation.x = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];

					if (tempAnim.jointInfo[i].flags & 16)	// orientation.y	( 010000 )
						tempFrameJoint.orientation.z = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];

					if (tempAnim.jointInfo[i].flags & 32)	// orientation.z	( 100000 )
						tempFrameJoint.orientation.y = tempFrame.frameData[tempAnim.jointInfo[i].startIndex + k++];


					// Compute the quaternions w
					float t = 1.0f - (tempFrameJoint.orientation.x * tempFrameJoint.orientation.x)
						- (tempFrameJoint.orientation.y * tempFrameJoint.orientation.y)
						- (tempFrameJoint.orientation.z * tempFrameJoint.orientation.z);
					if (t < 0.0f)
					{
						tempFrameJoint.orientation.w = 0.0f;
					}
					else
					{
						tempFrameJoint.orientation.w = -sqrtf(t);
					}

					// Now, if the upper arm of your skeleton moves, you need to also move the lower part of your arm, and then the hands, and then finally the fingers (possibly weapon or tool too)
					// This is where joint hierarchy comes in. We start at the top of the hierarchy, and move down to each joints child, rotating and translating them based on their parents rotation
					// and translation. We can assume that by the time we get to the child, the parent has already been rotated and transformed based of it's parent. We can assume this because
					// the child should never come before the parent in the files we loaded in.
					if (tempFrameJoint.parentID >= 0)
					{
						Joint parentJoint = tempSkeleton[tempFrameJoint.parentID];

						// Turn the XMFLOAT3 and 4's into vectors for easier computation
						XMVECTOR parentJointOrientation = XMVectorSet(parentJoint.orientation.x, parentJoint.orientation.y, parentJoint.orientation.z, parentJoint.orientation.w);
						XMVECTOR tempJointPos = XMVectorSet(tempFrameJoint.pos.x, tempFrameJoint.pos.y, tempFrameJoint.pos.z, 0.0f);
						XMVECTOR parentOrientationConjugate = XMVectorSet(-parentJoint.orientation.x, -parentJoint.orientation.y, -parentJoint.orientation.z, parentJoint.orientation.w);

						// Calculate current joints position relative to its parents position
						XMFLOAT3 rotatedPos;
						XMStoreFloat3(&rotatedPos, XMQuaternionMultiply(XMQuaternionMultiply(parentJointOrientation, tempJointPos), parentOrientationConjugate));

						// Translate the joint to model space by adding the parent joint's pos to it
						tempFrameJoint.pos.x = rotatedPos.x + parentJoint.pos.x;
						tempFrameJoint.pos.y = rotatedPos.y + parentJoint.pos.y;
						tempFrameJoint.pos.z = rotatedPos.z + parentJoint.pos.z;

						// Currently the joint is oriented in its parent joints space, we now need to orient it in
						// model space by multiplying the two orientations together (parentOrientation * childOrientation) <- In that order
						XMVECTOR tempJointOrient = XMVectorSet(tempFrameJoint.orientation.x, tempFrameJoint.orientation.y, tempFrameJoint.orientation.z, tempFrameJoint.orientation.w);
						tempJointOrient = XMQuaternionMultiply(parentJointOrientation, tempJointOrient);

						// Normalize the orienation quaternion
						tempJointOrient = XMQuaternionNormalize(tempJointOrient);

						XMStoreFloat4(&tempFrameJoint.orientation, tempJointOrient);
					}

					// Store the joint into our temporary frame skeleton
					tempSkeleton.push_back(tempFrameJoint);
				}

				// Push back our newly created frame skeleton into the animation's frameSkeleton array
				tempAnim.frameSkeleton.push_back(tempSkeleton);

				fileIn >> checkString;				// Skip closing bracket "}"
			}
		}

		// Calculate and store some usefull animation data
		tempAnim.frameTime = 1.0f / tempAnim.frameRate;						// Set the time per frame
		tempAnim.totalAnimTime = tempAnim.numFrames * tempAnim.frameTime;	// Set the total time the animation takes
		tempAnim.currAnimTime = 0.0f;										// Set the current time to zero

		std::wstring savefilename;
		WCHAR fname[MAX_PATH];
		WCHAR ext[MAX_PATH];

		_wsplitpath(filename.c_str(), NULL, NULL, fname, ext);

		savefilename = L".\\model\\";
		savefilename += fname;
		savefilename.append(ext);

		tempAnim.AniName = savefilename.c_str();

		MD5Model.animations.push_back(tempAnim);							// Push back the animation into our model object
		AniCount++;
	}
	else	// If the file was not loaded
	{
		SwapChain->SetFullscreenState(false, NULL);	// Make sure we are out of fullscreen

		// create message
		std::wstring message = L"Could not open: ";
		message += filename;

		MessageBox(0, message.c_str(),				// display message
			L"Error", MB_OK);

		return false;
	}
	return true;
}

void LoadMD5::UpdateMD5Model(float deltaTime, int animation)
{
	HRESULT hr;
	MD5Model.animations[animation].currAnimTime += deltaTime;			// Update the current animation time

	if (MD5Model.animations[animation].currAnimTime > MD5Model.animations[animation].totalAnimTime)
		MD5Model.animations[animation].currAnimTime = 0.0f;

	// Which frame are we on
	float currentFrame = MD5Model.animations[animation].currAnimTime * MD5Model.animations[animation].frameRate;
	int frame0 = floorf(currentFrame);
	int frame1 = frame0 + 1;

	// Make sure we don't go over the number of frames	
	if (frame0 == MD5Model.animations[animation].numFrames - 1)
		frame1 = 0;

	float interpolation = currentFrame - frame0;	// Get the remainder (in time) between frame0 and frame1 to use as interpolation factor

	std::vector<Joint> interpolatedSkeleton;		// Create a frame skeleton to store the interpolated skeletons in

	// Compute the interpolated skeleton
	for (int i = 0; i < MD5Model.animations[animation].numJoints; i++)
	{
		Joint tempJoint;
		Joint joint0 = MD5Model.animations[animation].frameSkeleton[frame0][i];		// Get the i'th joint of frame0's skeleton
		Joint joint1 = MD5Model.animations[animation].frameSkeleton[frame1][i];		// Get the i'th joint of frame1's skeleton

		tempJoint.parentID = joint0.parentID;											// Set the tempJoints parent id

		// Turn the two quaternions into XMVECTORs for easy computations
		XMVECTOR joint0Orient = XMVectorSet(joint0.orientation.x, joint0.orientation.y, joint0.orientation.z, joint0.orientation.w);
		XMVECTOR joint1Orient = XMVectorSet(joint1.orientation.x, joint1.orientation.y, joint1.orientation.z, joint1.orientation.w);

		// Interpolate positions
		tempJoint.pos.x = joint0.pos.x + (interpolation * (joint1.pos.x - joint0.pos.x));
		tempJoint.pos.y = joint0.pos.y + (interpolation * (joint1.pos.y - joint0.pos.y));
		tempJoint.pos.z = joint0.pos.z + (interpolation * (joint1.pos.z - joint0.pos.z));

		// Interpolate orientations using spherical interpolation (Slerp)
		XMStoreFloat4(&tempJoint.orientation, XMQuaternionSlerp(joint0Orient, joint1Orient, interpolation));

		interpolatedSkeleton.push_back(tempJoint);		// Push the joint back into our interpolated skeleton
	}

	for (int k = 0; k < MD5Model.numSubsets; k++)
	{
		for (int i = 0; i < MD5Model.subsets[k].vertices.size(); ++i)
		{
			Vertex tempVert = MD5Model.subsets[k].vertices[i];
			tempVert.pos = XMFLOAT3(0, 0, 0);	// Make sure the vertex's pos is cleared first
			tempVert.normal = XMFLOAT3(0, 0, 0);	// Clear vertices normal

			// Sum up the joints and weights information to get vertex's position and normal
			for (int j = 0; j < tempVert.WeightCount; ++j)
			{
				Weight tempWeight = MD5Model.subsets[k].weights[tempVert.StartWeight + j];
				Joint tempJoint = interpolatedSkeleton[tempWeight.jointID];

				// Convert joint orientation and weight pos to vectors for easier computation
				XMVECTOR tempJointOrientation = XMVectorSet(tempJoint.orientation.x, tempJoint.orientation.y, tempJoint.orientation.z, tempJoint.orientation.w);
				XMVECTOR tempWeightPos = XMVectorSet(tempWeight.pos.x, tempWeight.pos.y, tempWeight.pos.z, 0.0f);

				// We will need to use the conjugate of the joint orientation quaternion
				XMVECTOR tempJointOrientationConjugate = XMQuaternionInverse(tempJointOrientation);

				// Calculate vertex position (in joint space, eg. rotate the point around (0,0,0)) for this weight using the joint orientation quaternion and its conjugate
				// We can rotate a point using a quaternion with the equation "rotatedPoint = quaternion * point * quaternionConjugate"
				XMFLOAT3 rotatedPoint;
				XMStoreFloat3(&rotatedPoint, XMQuaternionMultiply(XMQuaternionMultiply(tempJointOrientation, tempWeightPos), tempJointOrientationConjugate));

				// Now move the verices position from joint space (0,0,0) to the joints position in world space, taking the weights bias into account
				tempVert.pos.x += (tempJoint.pos.x + rotatedPoint.x) * tempWeight.bias;
				tempVert.pos.y += (tempJoint.pos.y + rotatedPoint.y) * tempWeight.bias;
				tempVert.pos.z += (tempJoint.pos.z + rotatedPoint.z) * tempWeight.bias;

				// Compute the normals for this frames skeleton using the weight normals from before
				// We can comput the normals the same way we compute the vertices position, only we don't have to translate them (just rotate)
				XMVECTOR tempWeightNormal = XMVectorSet(tempWeight.normal.x, tempWeight.normal.y, tempWeight.normal.z, 0.0f);

				// Rotate the normal
				XMStoreFloat3(&rotatedPoint, XMQuaternionMultiply(XMQuaternionMultiply(tempJointOrientation, tempWeightNormal), tempJointOrientationConjugate));

				// Add to vertices normal and ake weight bias into account
				tempVert.normal.x -= rotatedPoint.x * tempWeight.bias;
				tempVert.normal.y -= rotatedPoint.y * tempWeight.bias;
				tempVert.normal.z -= rotatedPoint.z * tempWeight.bias;
			}

			MD5Model.subsets[k].positions[i] = tempVert.pos;				// Store the vertices position in the position vector instead of straight into the vertex vector
			MD5Model.subsets[k].vertices[i].normal = tempVert.normal;		// Store the vertices normal
			XMStoreFloat3(&MD5Model.subsets[k].vertices[i].normal, XMVector3Normalize(XMLoadFloat3(&MD5Model.subsets[k].vertices[i].normal)));
		}

		// Put the positions into the vertices for this subset
		for (int i = 0; i < MD5Model.subsets[k].vertices.size(); i++)
		{
			MD5Model.subsets[k].vertices[i].pos = MD5Model.subsets[k].positions[i];
		}

		// Update the subsets vertex buffer
		// First lock the buffer
		D3D11_MAPPED_SUBRESOURCE mappedVertBuff;
		hr = d3d11DevCon->Map(MD5Model.subsets[k].vertBuff, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertBuff);

		// Copy the data into the vertex buffer.
		memcpy(mappedVertBuff.pData, &MD5Model.subsets[k].vertices[0], (sizeof(Vertex)* MD5Model.subsets[k].vertices.size()));

		d3d11DevCon->Unmap(MD5Model.subsets[k].vertBuff, 0);

		// The line below is another way to update a buffer. You will use this when you want to update a buffer less
		// than once per frame, since the GPU reads will be faster (the buffer was created as a DEFAULT buffer instead
		// of a DYNAMIC buffer), and the CPU writes will be slower. You can try both methods to find out which one is faster
		// for you. if you want to use the line below, you will have to create the buffer with D3D11_USAGE_DEFAULT instead
		// of D3D11_USAGE_DYNAMIC
		//d3d11DevCon->UpdateSubresource( MD5Model.subsets[k].vertBuff, 0, NULL, &MD5Model.subsets[k].vertices[0], 0, 0 );
	}
}

LoadMain* LoadMD5::MakeClone()
{
	LoadMain* obj = new LoadMD5;

	dynamic_cast<LoadMD5*>(obj)->d3d11Device = d3d11Device;
	dynamic_cast<LoadMD5*>(obj)->d3d11DevCon = d3d11DevCon;
	dynamic_cast<LoadMD5*>(obj)->SwapChain = SwapChain;

	dynamic_cast<LoadMD5*>(obj)->TexSamplerState = TexSamplerState;
	dynamic_cast<LoadMD5*>(obj)->RSCullNone = RSCullNone;

	dynamic_cast<LoadMD5*>(obj)->meshSRV = meshSRV;
	dynamic_cast<LoadMD5*>(obj)->textureNameArray = textureNameArray;

	dynamic_cast<LoadMD5*>(obj)->MD5Model = MD5Model;

	for (int i = 0; i < MD5Model.subsets.size(); i++)
	{
		// Create index buffer
		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(DWORD)* MD5Model.subsets[i].numTriangles * 3;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA iinitData;

		iinitData.pSysMem = &MD5Model.subsets[i].indices[0];
		d3d11Device->CreateBuffer(&indexBufferDesc, &iinitData, &dynamic_cast<LoadMD5*>(obj)->MD5Model.subsets[i].indexBuff);

		//Create Vertex Buffer
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;							// We will be updating this buffer, so we must set as dynamic
		vertexBufferDesc.ByteWidth = sizeof(Vertex)* MD5Model.subsets[i].vertices.size();
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;				// Give CPU power to write to buffer
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = &MD5Model.subsets[i].vertices[0];
		d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &dynamic_cast<LoadMD5*>(obj)->MD5Model.subsets[i].vertBuff);
	}

	return obj;
}
///////////////**************new**************////////////////////
void LoadMD5::SetPos(float x, float y, float z)
{
	PositionValue.x = x;
	PositionValue.y = y;
	PositionValue.z = z;

	XMStoreFloat4x4(&Translation, XMMatrixTranslation(x, y, z));
}
void LoadMD5::SetRotation(float x, float y, float z)
{
	float xx = x * 0.0174532925f;
	float yy = y * 0.0174532925f;
	float zz = z * 0.0174532925f;
	RotationValue.x = x;
	RotationValue.y = y;
	RotationValue.z = z;

	XMStoreFloat4x4(&Rotation, XMMatrixRotationRollPitchYaw(xx, yy, zz));
}
void LoadMD5::SetScale(float x, float y, float z)
{
	ScaleValue.x = x;
	ScaleValue.y = y;
	ScaleValue.z = z;

	XMStoreFloat4x4(&Scale, XMMatrixScaling(x, y, z));
}
void LoadMD5::GetPosText(HWND hwnd)
{
	WCHAR text[3][MAX_PATH];

	swprintf(text[0], L"%lf", PositionValue.x);
	swprintf(text[1], L"%lf", PositionValue.y);
	swprintf(text[2], L"%lf", PositionValue.z);

	SetDlgItemText(hwnd, IDC_MD5POSX, text[0]);
	SetDlgItemText(hwnd, IDC_MD5POSY, text[1]);
	SetDlgItemText(hwnd, IDC_MD5POSZ, text[2]);
}
void LoadMD5::GetRotationText(HWND hwnd)
{
	WCHAR text[3][MAX_PATH];

	float xx = RotationValue.x;
	float yy = RotationValue.y;
	float zz = RotationValue.z;

	if (xx > 360.f)
	{
		xx = fmodf(xx, 360.f);
		RotationValue.x = xx;
	}
	if (yy > 360.f)
	{
		yy = fmodf(yy, 360.f);
		RotationValue.y = yy;
	}
	if (zz > 360.f)
	{
		zz = fmodf(zz, 360.f);
		RotationValue.z = zz;
	}

	swprintf(text[0], L"%lf", xx);
	swprintf(text[1], L"%lf", yy);
	swprintf(text[2], L"%lf", zz);

	SetDlgItemText(hwnd, IDC_MD5ROPITCH, text[0]);
	SetDlgItemText(hwnd, IDC_MD5ROYAW, text[1]);
	SetDlgItemText(hwnd, IDC_MD5ROROLL, text[2]);
}
void LoadMD5::GetScaleText(HWND hwnd)
{
	WCHAR text[3][MAX_PATH];

	swprintf(text[0], L"%lf", ScaleValue.x);
	swprintf(text[1], L"%lf", ScaleValue.y);
	swprintf(text[2], L"%lf", ScaleValue.z);

	SetDlgItemText(hwnd, IDC_MD5SCALEX, text[0]);
	SetDlgItemText(hwnd, IDC_MD5SCALEY, text[1]);
	SetDlgItemText(hwnd, IDC_MD5SCALEZ, text[2]);
}



float LoadMD5::pick(XMVECTOR pickRayInWorldSpacePos,
	XMVECTOR pickRayInWorldSpaceDir)
{
	XMMATRIX worldSpace = XMLoadFloat4x4(&MeshWorld);

	//Loop through each triangle in the object
	for (int o = 0; o < MD5Model.subsets.size(); o++)
	{
		for (int i = 0; i < MD5Model.subsets[o].indices.size() / 3; i++)
		{
			//Triangle's vertices V1, V2, V3
			XMVECTOR tri1V1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR tri1V2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR tri1V3 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

			//Temporary 3d floats for each vertex
			XMFLOAT3 tV1, tV2, tV3;
			
			//Get triangle 
			tV1 = MD5Model.subsets[o].vertices[MD5Model.subsets[o].indices[(i * 3) + 0]].pos;
			tV2 = MD5Model.subsets[o].vertices[MD5Model.subsets[o].indices[(i * 3) + 1]].pos;
			tV3 = MD5Model.subsets[o].vertices[MD5Model.subsets[o].indices[(i * 3) + 2]].pos;

			tri1V1 = XMVectorSet(tV1.x, tV1.y, tV1.z, 0.0f);
			tri1V2 = XMVectorSet(tV2.x, tV2.y, tV2.z, 0.0f);
			tri1V3 = XMVectorSet(tV3.x, tV3.y, tV3.z, 0.0f);

			//Transform the vertices to world space
			tri1V1 = XMVector3TransformCoord(tri1V1, worldSpace);
			tri1V2 = XMVector3TransformCoord(tri1V2, worldSpace);
			tri1V3 = XMVector3TransformCoord(tri1V3, worldSpace);

			//Find the normal using U, V coordinates (two edges)
			XMVECTOR U = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR V = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR faceNormal = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

			U = tri1V2 - tri1V1;
			V = tri1V3 - tri1V1;

			//Compute face normal by crossing U, V
			faceNormal = XMVector3Cross(U, V);

			faceNormal = XMVector3Normalize(faceNormal);

			//Calculate a point on the triangle for the plane equation
			XMVECTOR triPoint = tri1V1;

			//Get plane equation ("Ax + By + Cz + D = 0") Variables
			float tri1A = XMVectorGetX(faceNormal);
			float tri1B = XMVectorGetY(faceNormal);
			float tri1C = XMVectorGetZ(faceNormal);
			float tri1D = (-tri1A*XMVectorGetX(triPoint) - tri1B*XMVectorGetY(triPoint) - tri1C*XMVectorGetZ(triPoint));

			//Now we find where (on the ray) the ray intersects with the triangles plane
			float ep1, ep2, t = 0.0f;
			float planeIntersectX, planeIntersectY, planeIntersectZ = 0.0f;
			XMVECTOR pointInPlane = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

			ep1 = (XMVectorGetX(pickRayInWorldSpacePos) * tri1A) + (XMVectorGetY(pickRayInWorldSpacePos) * tri1B) + (XMVectorGetZ(pickRayInWorldSpacePos) * tri1C);
			ep2 = (XMVectorGetX(pickRayInWorldSpaceDir) * tri1A) + (XMVectorGetY(pickRayInWorldSpaceDir) * tri1B) + (XMVectorGetZ(pickRayInWorldSpaceDir) * tri1C);

			//Make sure there are no divide-by-zeros
			if (ep2 != 0.0f)
				t = -(ep1 + tri1D) / (ep2);

			if (t > 0.0f)    //Make sure you don't pick objects behind the camera
			{
				//Get the point on the plane
				planeIntersectX = XMVectorGetX(pickRayInWorldSpacePos) + XMVectorGetX(pickRayInWorldSpaceDir) * t;
				planeIntersectY = XMVectorGetY(pickRayInWorldSpacePos) + XMVectorGetY(pickRayInWorldSpaceDir) * t;
				planeIntersectZ = XMVectorGetZ(pickRayInWorldSpacePos) + XMVectorGetZ(pickRayInWorldSpaceDir) * t;

				pointInPlane = XMVectorSet(planeIntersectX, planeIntersectY, planeIntersectZ, 0.0f);

				//Call function to check if point is in the triangle
				if (PointInTriangle(tri1V1, tri1V2, tri1V3, pointInPlane))
				{
					//Return the distance to the hit, so you can check all the other pickable objects in your scene
					//and choose whichever object is closest to the camera
					return t / 2.0f;
				}
			}
		}
	}
	//return the max float value (near infinity) if an object was not picked
	return FLT_MAX;
}
bool LoadMD5::PointInTriangle(XMVECTOR& triV1, XMVECTOR& triV2, XMVECTOR& triV3, XMVECTOR& point)
{
	//To find out if the point is inside the triangle, we will check to see if the point
	//is on the correct side of each of the triangles edges.

	XMVECTOR cp1 = XMVector3Cross((triV3 - triV2), (point - triV2));
	XMVECTOR cp2 = XMVector3Cross((triV3 - triV2), (triV1 - triV2));
	if (XMVectorGetX(XMVector3Dot(cp1, cp2)) >= 0)
	{
		cp1 = XMVector3Cross((triV3 - triV1), (point - triV1));
		cp2 = XMVector3Cross((triV3 - triV1), (triV2 - triV1));
		if (XMVectorGetX(XMVector3Dot(cp1, cp2)) >= 0)
		{
			cp1 = XMVector3Cross((triV2 - triV1), (point - triV1));
			cp2 = XMVector3Cross((triV2 - triV1), (triV3 - triV1));
			if (XMVectorGetX(XMVector3Dot(cp1, cp2)) >= 0)
			{
				return true;
			}
			else
				return false;
		}
		else
			return false;
	}
	return false;
}
void LoadMD5::SetSaveTranfrom()
{
	SavePositionValue = PositionValue;
	SaveRotationValue = RotationValue;
	SaveScaleValue = ScaleValue;
}
void LoadMD5::ResetSaveTranfrom()
{
	SetPos(SavePositionValue.x, SavePositionValue.y, SavePositionValue.z);
	SetRotation(SaveRotationValue.x, SaveRotationValue.y, SaveRotationValue.z);
	SetScale(SaveScaleValue.x, SaveScaleValue.y, SaveScaleValue.z);
}