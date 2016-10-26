#include "LoadObj.h"


LoadObj::LoadObj()
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

LoadObj::~LoadObj()
{
}

void LoadObj::Setup(ID3D11Device* d3d11device, ID3D11DeviceContext* d3d11devCon, IDXGISwapChain* swapchain)
{
	d3d11Device = d3d11device;
	d3d11DevCon = d3d11devCon;
	SwapChain = swapchain;
}
void LoadObj::Init(ID3D11SamplerState* texsamplerstate,	ID3D11RasterizerState* rscullnone)
{
	TexSamplerState = texsamplerstate;
	RSCullNone = rscullnone;
}
void LoadObj::Update(const XMFLOAT4X4& camview, const XMFLOAT4X4& camprojection, const float& tick)
{
	XMMATRIX ro = XMLoadFloat4x4(&Rotation);
	XMMATRIX scale = XMLoadFloat4x4(&Scale);
	XMMATRIX translation = XMLoadFloat4x4(&Translation);

	XMMATRIX meshworld = ro * scale * translation;
	XMStoreFloat4x4(&MeshWorld, meshworld);

	camView = camview;
	camProjection = camprojection;
}
void LoadObj::DrawSceneToShadowMap(const XMFLOAT4X4& mLightView, const XMFLOAT4X4& mLightProj, const XMVECTOR& camerapos)
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

	d3d11DevCon->IASetInputLayout(InputLayouts::PosNormal);

	smapTech = Effects::BuildShadowMapFX->BuildShadowMapTech;
	smapTech->GetDesc(&techDesc);

	for (int i = 0; i < meshSubsets; ++i)
	{
		//Set the grounds index buffer
		d3d11DevCon->IASetIndexBuffer(meshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		d3d11DevCon->IASetVertexBuffers(0, 1, &meshVertBuff, &stride, &offset);

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = meshworld * view * proj;

		Effects::BuildShadowMapFX->SetWorld(meshworld);
		Effects::BuildShadowMapFX->SetWorldInvTranspose(XMMatrixTranspose(meshworld));
		Effects::BuildShadowMapFX->SetWorldViewProj(WVP);
		Effects::BuildShadowMapFX->SetTexTransform(XMMatrixIdentity());

		int indexStart = meshSubsetIndexStart[i];
		int indexDrawAmount = meshSubsetIndexStart[i + 1] - meshSubsetIndexStart[i];

		tessSmapTech->GetPassByIndex(i)->Apply(0, d3d11DevCon);

		if (!material[meshSubsetTexture[i]].transparent)
			d3d11DevCon->DrawIndexed(indexDrawAmount, indexStart, 0);
	}
}
void LoadObj::OnRender(const XMFLOAT4X4& shadowTransform)
{
	//Set Primitive Topology
	d3d11DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	XMMATRIX meshworld = XMLoadFloat4x4(&MeshWorld);
	XMMATRIX WVP;

	ID3DX11EffectTechnique* activeTech = Effects::BasicFX->Light1Tech;

	XMMATRIX mshadowTransform = XMLoadFloat4x4(&shadowTransform);

	D3DX11_TECHNIQUE_DESC techDesc;
	activeTech->GetDesc(&techDesc);

	for (int i = 0; i < meshSubsets; ++i)
	{
		//Set the grounds index buffer
		d3d11DevCon->IASetIndexBuffer(meshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		d3d11DevCon->IASetVertexBuffers(0, 1, &meshVertBuff, &stride, &offset);

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

		Effects::BasicFX->SethasTexture(material[meshSubsetTexture[i]].hasTexture);
		Effects::BasicFX->SethasNormal(material[meshSubsetTexture[i]].hasNormMap);

		/*if (material[meshSubsetTexture[i]].hasTexture)
			d3d11DevCon->PSSetShaderResources(0, 1, &meshSRV[material[meshSubsetTexture[i]].texArrayIndex]);*/
		if (material[meshSubsetTexture[i]].hasTexture)
			Effects::BasicFX->SetDiffuseMap(meshSRV[material[meshSubsetTexture[i]].texArrayIndex]);
		///////////////**************new**************////////////////////
		if (material[meshSubsetTexture[i]].hasNormMap)
			Effects::BasicFX->SetNormalMap(meshSRV[material[meshSubsetTexture[i]].normMapTexArrayIndex]);
		///////////////**************new**************////////////////////
		//d3d11DevCon->PSSetSamplers(0, 1, &TexSamplerState);

		d3d11DevCon->RSSetState(RSCullNone);
		int indexStart = meshSubsetIndexStart[i];
		int indexDrawAmount = meshSubsetIndexStart[i + 1] - meshSubsetIndexStart[i];

		activeTech->GetPassByIndex(i)->Apply(0, d3d11DevCon);

		if (!material[meshSubsetTexture[i]].transparent)
			d3d11DevCon->DrawIndexed(indexDrawAmount, indexStart, 0);
	}
}

bool LoadObj::LoadObjModel(std::wstring filename,						
	bool isRHCoordSys,							
	bool computeNormals)
{
	HRESULT hr = 0;

	std::wifstream fileIn(filename);	//Open file
	std::wstring meshMatLib;					//String to hold our obj material library filename

	//Arrays to store our model's information
	std::vector<DWORD> indices;
	std::vector<XMFLOAT3> vertPos;
	std::vector<XMFLOAT3> vertNorm;
	std::vector<XMFLOAT2> vertTexCoord;
	std::vector<std::wstring> meshMaterials;

	//Vertex definition indices
	std::vector<int> vertPosIndex;
	std::vector<int> vertNormIndex;
	std::vector<int> vertTCIndex;

	//Make sure we have a default if no tex coords or normals are defined
	bool hasTexCoord = false;
	bool hasNorm = false;

	//Temp variables to store into vectors
	std::wstring meshMaterialsTemp;
	int vertPosIndexTemp;
	int vertNormIndexTemp;
	int vertTCIndexTemp;

	wchar_t checkChar;		//The variable we will use to store one char from file at a time
	std::wstring face;		//Holds the string containing our face vertices
	int vIndex = 0;			//Keep track of our vertex index count
	int triangleCount = 0;	//Total Triangles
	int totalVerts = 0;
	int meshTriangles = 0;

	//Check to see if the file was opened
	if (fileIn.good())
	{
		while (fileIn)
		{
			checkChar = fileIn.get();	//Get next char

			switch (checkChar)
			{
			case '#':
				checkChar = fileIn.get();
				while (checkChar != '\n')
					checkChar = fileIn.get();
				break;
			case 'v':	//Get Vertex Descriptions
				checkChar = fileIn.get();
				if (checkChar == ' ')	//v - vert position
				{
					float vz, vy, vx;
					fileIn >> vx >> vy >> vz;	//Store the next three types

					if (isRHCoordSys)	//If model is from an RH Coord System
						vertPos.push_back(XMFLOAT3(vx, vy, vz * -1.0f));	//Invert the Z axis
					else
						vertPos.push_back(XMFLOAT3(vx, vy, vz));
				}
				if (checkChar == 't')	//vt - vert tex coords
				{
					float vtcu, vtcv;
					fileIn >> vtcu >> vtcv;		//Store next two types

					if (isRHCoordSys)	//If model is from an RH Coord System
						vertTexCoord.push_back(XMFLOAT2(vtcu, 1.0f - vtcv));	//Reverse the "v" axis
					else
						vertTexCoord.push_back(XMFLOAT2(vtcu, vtcv));

					hasTexCoord = true;	//We know the model uses texture coords
				}
				//Since we compute the normals later, we don't need to check for normals
				//In the file, but i'll do it here anyway
				if (checkChar == 'n')	//vn - vert normal
				{
					float vnx, vny, vnz;
					fileIn >> vnx >> vny >> vnz;	//Store next three types

					if (isRHCoordSys)	//If model is from an RH Coord System
						vertNorm.push_back(XMFLOAT3(vnx, vny, vnz * -1.0f));	//Invert the Z axis
					else
						vertNorm.push_back(XMFLOAT3(vnx, vny, vnz));

					hasNorm = true;	//We know the model defines normals
				}
				break;

				//New group (Subset)
			case 'g':	//g - defines a group
				checkChar = fileIn.get();
				if (checkChar == ' ')
				{
					meshSubsetIndexStart.push_back(vIndex);		//Start index for this subset
					meshSubsets++;
				}
				break;

				//Get Face Index
			case 'f':	//f - defines the faces
				checkChar = fileIn.get();
				if (checkChar == ' ')
				{
					face = L"";
					std::wstring VertDef;	//Holds one vertex definition at a time
					triangleCount = 0;

					checkChar = fileIn.get();
					while (checkChar != '\n')
					{
						face += checkChar;			//Add the char to our face string
						checkChar = fileIn.get();	//Get the next Character
						if (checkChar == ' ')		//If its a space...
							triangleCount++;		//Increase our triangle count
					}

					//Check for space at the end of our face string
					if (face[face.length() - 1] == ' ')
						triangleCount--;	//Each space adds to our triangle count

					triangleCount -= 1;		//Ever vertex in the face AFTER the first two are new faces

					std::wstringstream ss(face);

					if (face.length() > 0)
					{
						int firstVIndex, lastVIndex;	//Holds the first and last vertice's index

						for (int i = 0; i < 3; ++i)		//First three vertices (first triangle)
						{
							ss >> VertDef;	//Get vertex definition (vPos/vTexCoord/vNorm)

							std::wstring vertPart;
							int whichPart = 0;		//(vPos, vTexCoord, or vNorm)

							//Parse this string
							for (int j = 0; j < VertDef.length(); ++j)
							{
								if (VertDef[j] != '/')	//If there is no divider "/", add a char to our vertPart
									vertPart += VertDef[j];

								//If the current char is a divider "/", or its the last character in the string
								if (VertDef[j] == '/' || j == VertDef.length() - 1)
								{
									std::wistringstream wstringToInt(vertPart);	//Used to convert wstring to int

									if (whichPart == 0)	//If vPos
									{
										wstringToInt >> vertPosIndexTemp;
										vertPosIndexTemp -= 1;		//subtract one since c++ arrays start with 0, and obj start with 1

										//Check to see if the vert pos was the only thing specified
										if (j == VertDef.length() - 1)
										{
											vertNormIndexTemp = 0;
											vertTCIndexTemp = 0;
										}
									}

									else if (whichPart == 1)	//If vTexCoord
									{
										if (vertPart != L"")	//Check to see if there even is a tex coord
										{
											wstringToInt >> vertTCIndexTemp;
											vertTCIndexTemp -= 1;	//subtract one since c++ arrays start with 0, and obj start with 1
										}
										else	//If there is no tex coord, make a default
											vertTCIndexTemp = 0;

										//If the cur. char is the second to last in the string, then
										//there must be no normal, so set a default normal
										if (j == VertDef.length() - 1)
											vertNormIndexTemp = 0;

									}
									else if (whichPart == 2)	//If vNorm
									{
										std::wistringstream wstringToInt(vertPart);

										wstringToInt >> vertNormIndexTemp;
										vertNormIndexTemp -= 1;		//subtract one since c++ arrays start with 0, and obj start with 1
									}

									vertPart = L"";	//Get ready for next vertex part
									whichPart++;	//Move on to next vertex part					
								}
							}

							//Check to make sure there is at least one subset
							if (meshSubsets == 0)
							{
								meshSubsetIndexStart.push_back(vIndex);		//Start index for this subset
								meshSubsets++;
							}

							//Avoid duplicate vertices
							bool vertAlreadyExists = false;
							if (totalVerts >= 3)	//Make sure we at least have one triangle to check
							{
								//Loop through all the vertices
								for (int iCheck = 0; iCheck < totalVerts; ++iCheck)
								{
									//If the vertex position and texture coordinate in memory are the same
									//As the vertex position and texture coordinate we just now got out
									//of the obj file, we will set this faces vertex index to the vertex's
									//index value in memory. This makes sure we don't create duplicate vertices
									if (vertPosIndexTemp == vertPosIndex[iCheck] && !vertAlreadyExists)
									{
										if (vertTCIndexTemp == vertTCIndex[iCheck])
										{
											indices.push_back(iCheck);		//Set index for this vertex
											vertAlreadyExists = true;		//If we've made it here, the vertex already exists
										}
									}
								}
							}

							//If this vertex is not already in our vertex arrays, put it there
							if (!vertAlreadyExists)
							{
								vertPosIndex.push_back(vertPosIndexTemp);
								vertTCIndex.push_back(vertTCIndexTemp);
								vertNormIndex.push_back(vertNormIndexTemp);
								totalVerts++;	//We created a new vertex
								indices.push_back(totalVerts - 1);	//Set index for this vertex
							}

							//If this is the very first vertex in the face, we need to
							//make sure the rest of the triangles use this vertex
							if (i == 0)
							{
								firstVIndex = indices[vIndex];	//The first vertex index of this FACE

							}

							//If this was the last vertex in the first triangle, we will make sure
							//the next triangle uses this one (eg. tri1(1,2,3) tri2(1,3,4) tri3(1,4,5))
							if (i == 2)
							{
								lastVIndex = indices[vIndex];	//The last vertex index of this TRIANGLE
							}
							vIndex++;	//Increment index count
						}

						meshTriangles++;	//One triangle down

						//If there are more than three vertices in the face definition, we need to make sure
						//we convert the face to triangles. We created our first triangle above, now we will
						//create a new triangle for every new vertex in the face, using the very first vertex
						//of the face, and the last vertex from the triangle before the current triangle
						for (int l = 0; l < triangleCount - 1; ++l)	//Loop through the next vertices to create new triangles
						{
							//First vertex of this triangle (the very first vertex of the face too)
							indices.push_back(firstVIndex);			//Set index for this vertex
							vIndex++;

							//Second Vertex of this triangle (the last vertex used in the tri before this one)
							indices.push_back(lastVIndex);			//Set index for this vertex
							vIndex++;

							//Get the third vertex for this triangle
							ss >> VertDef;

							std::wstring vertPart;
							int whichPart = 0;

							//Parse this string (same as above)
							for (int j = 0; j < VertDef.length(); ++j)
							{
								if (VertDef[j] != '/')
									vertPart += VertDef[j];
								if (VertDef[j] == '/' || j == VertDef.length() - 1)
								{
									std::wistringstream wstringToInt(vertPart);

									if (whichPart == 0)
									{
										wstringToInt >> vertPosIndexTemp;
										vertPosIndexTemp -= 1;

										//Check to see if the vert pos was the only thing specified
										if (j == VertDef.length() - 1)
										{
											vertTCIndexTemp = 0;
											vertNormIndexTemp = 0;
										}
									}
									else if (whichPart == 1)
									{
										if (vertPart != L"")
										{
											wstringToInt >> vertTCIndexTemp;
											vertTCIndexTemp -= 1;
										}
										else
											vertTCIndexTemp = 0;
										if (j == VertDef.length() - 1)
											vertNormIndexTemp = 0;

									}
									else if (whichPart == 2)
									{
										std::wistringstream wstringToInt(vertPart);

										wstringToInt >> vertNormIndexTemp;
										vertNormIndexTemp -= 1;
									}

									vertPart = L"";
									whichPart++;
								}
							}

							//Check for duplicate vertices
							bool vertAlreadyExists = false;
							if (totalVerts >= 3)	//Make sure we at least have one triangle to check
							{
								for (int iCheck = 0; iCheck < totalVerts; ++iCheck)
								{
									if (vertPosIndexTemp == vertPosIndex[iCheck] && !vertAlreadyExists)
									{
										if (vertTCIndexTemp == vertTCIndex[iCheck])
										{
											indices.push_back(iCheck);			//Set index for this vertex
											vertAlreadyExists = true;		//If we've made it here, the vertex already exists
										}
									}
								}
							}

							if (!vertAlreadyExists)
							{
								vertPosIndex.push_back(vertPosIndexTemp);
								vertTCIndex.push_back(vertTCIndexTemp);
								vertNormIndex.push_back(vertNormIndexTemp);
								totalVerts++;					//New vertex created, add to total verts
								indices.push_back(totalVerts - 1);		//Set index for this vertex
							}

							//Set the second vertex for the next triangle to the last vertex we got		
							lastVIndex = indices[vIndex];	//The last vertex index of this TRIANGLE

							meshTriangles++;	//New triangle defined
							vIndex++;
						}
					}
				}
				break;

			case 'm':	//mtllib - material library filename
				checkChar = fileIn.get();
				if (checkChar == 't')
				{
					checkChar = fileIn.get();
					if (checkChar == 'l')
					{
						checkChar = fileIn.get();
						if (checkChar == 'l')
						{
							checkChar = fileIn.get();
							if (checkChar == 'i')
							{
								checkChar = fileIn.get();
								if (checkChar == 'b')
								{
									checkChar = fileIn.get();
									if (checkChar == ' ')
									{
										//Store the material libraries file name
										fileIn >> meshMatLib;
									}
								}
							}
						}
					}
				}

				break;

			case 'u':	//usemtl - which material to use
				checkChar = fileIn.get();
				if (checkChar == 's')
				{
					checkChar = fileIn.get();
					if (checkChar == 'e')
					{
						checkChar = fileIn.get();
						if (checkChar == 'm')
						{
							checkChar = fileIn.get();
							if (checkChar == 't')
							{
								checkChar = fileIn.get();
								if (checkChar == 'l')
								{
									checkChar = fileIn.get();
									if (checkChar == ' ')
									{
										meshMaterialsTemp = L"";	//Make sure this is cleared

										fileIn >> meshMaterialsTemp; //Get next type (string)

										meshMaterials.push_back(meshMaterialsTemp);
									}
								}
							}
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}
	else	//If we could not open the file
	{
		SwapChain->SetFullscreenState(false, NULL);	//Make sure we are out of fullscreen

		//create message
		std::wstring message = L"Could not open: ";
		message += filename;

		MessageBox(0, message.c_str(),	//display message
			L"Error", MB_OK);

		return false;
	}

	meshSubsetIndexStart.push_back(vIndex); //There won't be another index start after our last subset, so set it here

	//sometimes "g" is defined at the very top of the file, then again before the first group of faces.
	//This makes sure the first subset does not conatain "0" indices.
	if (meshSubsetIndexStart[1] == 0)
	{
		meshSubsetIndexStart.erase(meshSubsetIndexStart.begin() + 1);
		meshSubsets--;
	}

	//Make sure we have a default for the tex coord and normal
	//if one or both are not specified
	if (!hasNorm)
		vertNorm.push_back(XMFLOAT3(0.0f, 0.0f, 0.0f));
	if (!hasTexCoord)
		vertTexCoord.push_back(XMFLOAT2(0.0f, 0.0f));

	//Close the obj file, and open the mtl file
	fileIn.close();

	std::wstring wmeshroot;
	wmeshroot = L".\\model\\";
	wmeshroot.append(meshMatLib);

	fileIn.open(wmeshroot.c_str());

	std::wstring lastStringRead;
	int matCount = material.size();	//total materials

	//kdset - If our diffuse color was not set, we can use the ambient color (which is usually the same)
	//If the diffuse color WAS set, then we don't need to set our diffuse color to ambient
	bool kdset = false;

	if (fileIn.good())
	{
		while (fileIn)
		{
			checkChar = fileIn.get();	//Get next char

			switch (checkChar)
			{
				//Check for comment
			case '#':
				checkChar = fileIn.get();
				while (checkChar != '\n')
					checkChar = fileIn.get();
				break;

				//Set diffuse color
			case 'K':
				checkChar = fileIn.get();
				if (checkChar == 'd')	//Diffuse Color
				{
					checkChar = fileIn.get();	//remove space

					fileIn >> material[matCount - 1].difColor.x;
					fileIn >> material[matCount - 1].difColor.y;
					fileIn >> material[matCount - 1].difColor.z;

					kdset = true;
				}

				//Ambient Color (We'll store it in diffuse if there isn't a diffuse already)
				if (checkChar == 'a')
				{
					checkChar = fileIn.get();	//remove space
					if (!kdset)
					{
						fileIn >> material[matCount - 1].difColor.x;
						fileIn >> material[matCount - 1].difColor.y;
						fileIn >> material[matCount - 1].difColor.z;
					}
				}
				break;

				//Check for transparency
			case 'T':
				checkChar = fileIn.get();
				if (checkChar == 'r')
				{
					checkChar = fileIn.get();	//remove space
					float Transparency;
					fileIn >> Transparency;

					material[matCount - 1].difColor.w = Transparency;

					if (Transparency > 0.0f)
						material[matCount - 1].transparent = true;
				}
				break;

				//Some obj files specify d for transparency
			case 'd':
				checkChar = fileIn.get();
				if (checkChar == ' ')
				{
					float Transparency;
					fileIn >> Transparency;

					//'d' - 0 being most transparent, and 1 being opaque, opposite of Tr
					Transparency = 1.0f - Transparency;

					material[matCount - 1].difColor.w = Transparency;

					if (Transparency > 0.0f)
						material[matCount - 1].transparent = true;
				}
				break;

				//Get the diffuse map (texture)
			case 'm':
				checkChar = fileIn.get();
				if (checkChar == 'a')
				{
					checkChar = fileIn.get();
					if (checkChar == 'p')
					{
						checkChar = fileIn.get();
						if (checkChar == '_')
						{
							//map_Kd - Diffuse map
							checkChar = fileIn.get();
							if (checkChar == 'K')
							{
								checkChar = fileIn.get();
								if (checkChar == 'd')
								{
									std::wstring fileNamePath;

									fileIn.get();	//Remove whitespace between map_Kd and file

									//Get the file path - We read the pathname char by char since
									//pathnames can sometimes contain spaces, so we will read until
									//we find the file extension
									bool texFilePathEnd = false;
									while (!texFilePathEnd)
									{
										checkChar = fileIn.get();
										fileNamePath += checkChar;

										if (checkChar == '.')
										{
											for (int i = 0; i < 3; ++i)
												fileNamePath += fileIn.get();

											texFilePathEnd = true;
										}
									}

									//check if this texture has already been loaded
									bool alreadyLoaded = false;
									for (int i = 0; i < textureNameArray.size(); ++i)
									{
										if (fileNamePath == textureNameArray[i])
										{
											alreadyLoaded = true;
											material[matCount - 1].texArrayIndex = i;
											material[matCount - 1].hasTexture = true;
										}
									}

									//if the texture is not already loaded, load it now
									if (!alreadyLoaded)
									{
										ID3D11ShaderResourceView* tempMeshSRV;
										std::wstring pluspath = L".\\model\\";
										pluspath += fileNamePath;
										hr = D3DX11CreateShaderResourceViewFromFile(d3d11Device, pluspath.c_str(),
											NULL, NULL, &tempMeshSRV, NULL);
										if (SUCCEEDED(hr))
										{
											textureNameArray.push_back(pluspath.c_str());
											material[matCount - 1].texArrayIndex = meshSRV.size();
											meshSRV.push_back(tempMeshSRV);
											material[matCount - 1].hasTexture = true;
										}
									}
								}
							}
							//map_d - alpha map
							else if (checkChar == 'd')
							{
								//Alpha maps are usually the same as the diffuse map
								//So we will assume that for now by only enabling
								//transparency for this material, as we will already
								//be using the alpha channel in the diffuse map
								material[matCount - 1].transparent = true;
							}
						}
						///////////////**************new**************////////////////////
						//map_bump - bump map (we're usinga normal map though)
						else if (checkChar == 'b')
						{
							checkChar = fileIn.get();
							if (checkChar == 'u')
							{
								checkChar = fileIn.get();
								if (checkChar == 'm')
								{
									checkChar = fileIn.get();
									if (checkChar == 'p')
									{
										std::wstring fileNamePath;

										fileIn.get();	//Remove whitespace between map_bump and file

										//Get the file path - We read the pathname char by char since
										//pathnames can sometimes contain spaces, so we will read until
										//we find the file extension
										bool texFilePathEnd = false;
										while (!texFilePathEnd)
										{
											checkChar = fileIn.get();

											fileNamePath += checkChar;

											if (checkChar == '.')
											{
												for (int i = 0; i < 3; ++i)
													fileNamePath += fileIn.get();

												texFilePathEnd = true;
											}
										}

										//check if this texture has already been loaded
										bool alreadyLoaded = false;
										for (int i = 0; i < textureNameArray.size(); ++i)
										{
											if (fileNamePath == textureNameArray[i])
											{
												alreadyLoaded = true;
												material[matCount - 1].normMapTexArrayIndex = i;
												material[matCount - 1].hasNormMap = true;
											}
										}

										//if the texture is not already loaded, load it now
										if (!alreadyLoaded)
										{
											ID3D11ShaderResourceView* tempMeshSRV;
											hr = D3DX11CreateShaderResourceViewFromFile(d3d11Device, fileNamePath.c_str(),
												NULL, NULL, &tempMeshSRV, NULL);
											if (SUCCEEDED(hr))
											{
												textureNameArray.push_back(fileNamePath.c_str());
												material[matCount - 1].normMapTexArrayIndex = meshSRV.size();
												meshSRV.push_back(tempMeshSRV);
												material[matCount - 1].hasNormMap = true;
											}
										}
									}
								}
							}
						}
						///////////////**************new**************////////////////////
					}
				}
				break;

			case 'n':	//newmtl - Declare new material
				checkChar = fileIn.get();
				if (checkChar == 'e')
				{
					checkChar = fileIn.get();
					if (checkChar == 'w')
					{
						checkChar = fileIn.get();
						if (checkChar == 'm')
						{
							checkChar = fileIn.get();
							if (checkChar == 't')
							{
								checkChar = fileIn.get();
								if (checkChar == 'l')
								{
									checkChar = fileIn.get();
									if (checkChar == ' ')
									{
										//New material, set its defaults
										SurfaceMaterial tempMat;
										material.push_back(tempMat);
										fileIn >> material[matCount].matName;
										material[matCount].transparent = false;
										material[matCount].hasTexture = false;
										///////////////**************new**************////////////////////
										material[matCount].hasNormMap = false;
										material[matCount].normMapTexArrayIndex = 0;
										///////////////**************new**************////////////////////
										material[matCount].texArrayIndex = 0;
										matCount++;
										kdset = false;
									}
								}
							}
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}
	else
	{
		SwapChain->SetFullscreenState(false, NULL);	//Make sure we are out of fullscreen

		std::wstring message = L"Could not open: ";
		message += wmeshroot;

		MessageBox(0, message.c_str(),
			L"Error", MB_OK);

		return false;
	}

	//Set the subsets material to the index value
	//of the its material in our material array
	for (int i = 0; i < meshSubsets; ++i)
	{
		bool hasMat = false;
		for (int j = 0; j < material.size(); ++j)
		{
			if (meshMaterials[i] == material[j].matName)
			{
				meshSubsetTexture.push_back(j);
				hasMat = true;
			}
		}
		if (!hasMat)
			meshSubsetTexture.push_back(0); //Use first material in array
	}

	std::vector<Vertex> vertices;
	Vertex tempVert;

	//Create our vertices using the information we got 
	//from the file and store them in a vector
	for (int j = 0; j < totalVerts; ++j)
	{
		tempVert.pos = vertPos[vertPosIndex[j]];
		tempVert.normal = vertNorm[vertNormIndex[j]];
		tempVert.texCoord = vertTexCoord[vertTCIndex[j]];

		vertices.push_back(tempVert);

		vertPosArray.push_back(tempVert.pos);
	}

	indexPosArray = indices;

	//////////////////////Compute Normals///////////////////////////
	//If computeNormals was set to true then we will create our own
	//normals, if it was set to false we will use the obj files normals
	if (computeNormals)
	{
		std::vector<XMFLOAT3> tempNormal;

		//normalized and unnormalized normals
		XMFLOAT3 unnormalized = XMFLOAT3(0.0f, 0.0f, 0.0f);

		///////////////**************new**************////////////////////
		//tangent stuff
		std::vector<XMFLOAT3> tempTangent;
		XMFLOAT3 tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float tcU1, tcV1, tcU2, tcV2;
		///////////////**************new**************////////////////////

		//Used to get vectors (sides) from the position of the verts
		float vecX, vecY, vecZ;

		//Two edges of our triangle
		XMVECTOR edge1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR edge2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

		//Compute face normals
		for (int i = 0; i < meshTriangles; ++i)
		{
			//Get the vector describing one edge of our triangle (edge 0,2)
			vecX = vertices[indices[(i * 3)]].pos.x - vertices[indices[(i * 3) + 2]].pos.x;
			vecY = vertices[indices[(i * 3)]].pos.y - vertices[indices[(i * 3) + 2]].pos.y;
			vecZ = vertices[indices[(i * 3)]].pos.z - vertices[indices[(i * 3) + 2]].pos.z;
			edge1 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our first edge

			//Get the vector describing another edge of our triangle (edge 2,1)
			vecX = vertices[indices[(i * 3) + 2]].pos.x - vertices[indices[(i * 3) + 1]].pos.x;
			vecY = vertices[indices[(i * 3) + 2]].pos.y - vertices[indices[(i * 3) + 1]].pos.y;
			vecZ = vertices[indices[(i * 3) + 2]].pos.z - vertices[indices[(i * 3) + 1]].pos.z;
			edge2 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our second edge

			//Cross multiply the two edge vectors to get the un-normalized face normal
			XMStoreFloat3(&unnormalized, XMVector3Cross(edge1, edge2));
			tempNormal.push_back(unnormalized);			//Save unormalized normal (for normal averaging)

			///////////////**************new**************////////////////////
			//Find first texture coordinate edge 2d vector
			tcU1 = vertices[indices[(i * 3)]].texCoord.x - vertices[indices[(i * 3) + 2]].texCoord.x;
			tcV1 = vertices[indices[(i * 3)]].texCoord.y - vertices[indices[(i * 3) + 2]].texCoord.y;

			//Find second texture coordinate edge 2d vector
			tcU2 = vertices[indices[(i * 3) + 2]].texCoord.x - vertices[indices[(i * 3) + 1]].texCoord.x;
			tcV2 = vertices[indices[(i * 3) + 2]].texCoord.y - vertices[indices[(i * 3) + 1]].texCoord.y;

			//Find tangent using both tex coord edges and position edges
			tangent.x = (tcV1 * XMVectorGetX(edge1) - tcV2 * XMVectorGetX(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));
			tangent.y = (tcV1 * XMVectorGetY(edge1) - tcV2 * XMVectorGetY(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));
			tangent.z = (tcV1 * XMVectorGetZ(edge1) - tcV2 * XMVectorGetZ(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));

			tempTangent.push_back(tangent);
			///////////////**************new**************////////////////////
		}

		//Compute vertex normals (normal Averaging)
		XMVECTOR normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR tangentSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		int facesUsing = 0;
		float tX;
		float tY;
		float tZ;

		//Go through each vertex
		for (int i = 0; i < totalVerts; ++i)
		{
			//Check which triangles use this vertex
			for (int j = 0; j < meshTriangles; ++j)
			{
				if (indices[j * 3] == i ||
					indices[(j * 3) + 1] == i ||
					indices[(j * 3) + 2] == i)
				{
					tX = XMVectorGetX(normalSum) + tempNormal[j].x;
					tY = XMVectorGetY(normalSum) + tempNormal[j].y;
					tZ = XMVectorGetZ(normalSum) + tempNormal[j].z;

					normalSum = XMVectorSet(tX, tY, tZ, 0.0f);	//If a face is using the vertex, add the unormalized face normal to the normalSum
					
					///////////////**************new**************////////////////////		
					//We can reuse tX, tY, tZ to sum up tangents
					tX = XMVectorGetX(tangentSum) + tempTangent[j].x;
					tY = XMVectorGetY(tangentSum) + tempTangent[j].y;
					tZ = XMVectorGetZ(tangentSum) + tempTangent[j].z;

					tangentSum = XMVectorSet(tX, tY, tZ, 0.0f); //sum up face tangents using this vertex
					///////////////**************new**************////////////////////

					facesUsing++;
				}
			}

			//Get the actual normal by dividing the normalSum by the number of faces sharing the vertex
			normalSum = normalSum / facesUsing;
			///////////////**************new**************////////////////////
			tangentSum = tangentSum / facesUsing;
			///////////////**************new**************////////////////////

			//Normalize the normalSum vector
			normalSum = XMVector3Normalize(normalSum);
			///////////////**************new**************////////////////////
			tangentSum = XMVector3Normalize(tangentSum);
			///////////////**************new**************////////////////////

			//Store the normal in our current vertex
			vertices[i].normal.x = XMVectorGetX(normalSum);
			vertices[i].normal.y = XMVectorGetY(normalSum);
			vertices[i].normal.z = XMVectorGetZ(normalSum);

			///////////////**************new**************////////////////////
			vertices[i].tangent.x = XMVectorGetX(tangentSum);
			vertices[i].tangent.y = XMVectorGetY(tangentSum);
			vertices[i].tangent.z = XMVectorGetZ(tangentSum);
			///////////////**************new**************////////////////////

			//Clear normalSum and facesUsing for next vertex
			normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			///////////////**************new**************////////////////////
			tangentSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			///////////////**************new**************////////////////////
			facesUsing = 0;

		}
	}

	//Create index buffer
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD)* meshTriangles * 3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = &indices[0];
	d3d11Device->CreateBuffer(&indexBufferDesc, &iinitData, &meshIndexBuff);

	//Create Vertex Buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(Vertex)* totalVerts;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;

	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = &vertices[0];

	hr = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &meshVertBuff);

	int aasd;

	return true;
}
LoadMain* LoadObj::MakeClone()
{
	LoadMain* obj = new LoadObj;

	dynamic_cast<LoadObj*>(obj)->d3d11Device = d3d11Device;
	dynamic_cast<LoadObj*>(obj)->d3d11DevCon = d3d11DevCon;
	dynamic_cast<LoadObj*>(obj)->SwapChain = SwapChain;

	dynamic_cast<LoadObj*>(obj)->TexSamplerState = TexSamplerState;
	dynamic_cast<LoadObj*>(obj)->RSCullNone = RSCullNone;

	dynamic_cast<LoadObj*>(obj)->meshVertBuff = meshVertBuff;
	dynamic_cast<LoadObj*>(obj)->meshIndexBuff = meshIndexBuff;

	dynamic_cast<LoadObj*>(obj)->meshSubsets = meshSubsets;
	dynamic_cast<LoadObj*>(obj)->meshSubsetIndexStart = meshSubsetIndexStart;
	dynamic_cast<LoadObj*>(obj)->meshSubsetTexture = meshSubsetTexture;
	dynamic_cast<LoadObj*>(obj)->material = material;
	dynamic_cast<LoadObj*>(obj)->meshSRV = meshSRV;
	dynamic_cast<LoadObj*>(obj)->textureNameArray = textureNameArray;

	dynamic_cast<LoadObj*>(obj)->vertPosArray = vertPosArray;
	dynamic_cast<LoadObj*>(obj)->indexPosArray = indexPosArray;

	return obj;
}
void LoadObj::CleanUp()
{
	if (meshVertBuff != NULL)
		meshVertBuff->Release();
	if (meshIndexBuff != NULL)
		meshIndexBuff->Release();
}
void LoadObj::SetPos(float x, float y, float z)
{ 
	PositionValue.x = x;
	PositionValue.y = y; 
	PositionValue.z = z;

	XMStoreFloat4x4(&Translation, XMMatrixTranslation(x, y, z));
}
void LoadObj::SetRotation(float x, float y, float z) 
{
	float xx = x * 0.0174532925f;
	float yy = y * 0.0174532925f;
	float zz = z * 0.0174532925f;
	RotationValue.x = x;
	RotationValue.y = y;
	RotationValue.z = z;

	XMStoreFloat4x4(&Rotation, XMMatrixRotationRollPitchYaw(xx, yy, zz));
}
void LoadObj::SetScale(float x, float y, float z) 
{
	ScaleValue.x = x; 
	ScaleValue.y = y;
	ScaleValue.z = z;

	XMStoreFloat4x4(&Scale, XMMatrixScaling(x, y, z));
}
void LoadObj::GetPosText(HWND hwnd)
{
	WCHAR text[3][MAX_PATH];
	
	swprintf(text[0], L"%lf", PositionValue.x);
	swprintf(text[1], L"%lf", PositionValue.y);
	swprintf(text[2], L"%lf", PositionValue.z);

	SetDlgItemText(hwnd, IDC_OBJPOSX, text[0]);
	SetDlgItemText(hwnd, IDC_OBJPOSY, text[1]);
	SetDlgItemText(hwnd, IDC_OBJPOSZ, text[2]);
}
void LoadObj::GetRotationText(HWND hwnd)
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

	SetDlgItemText(hwnd, IDC_OBJROPITCH, text[0]);
	SetDlgItemText(hwnd, IDC_OBJROYAW, text[1]);
	SetDlgItemText(hwnd, IDC_OBJROROLL, text[2]);
}
void LoadObj::GetScaleText(HWND hwnd)
{
	WCHAR text[3][MAX_PATH];

	swprintf(text[0], L"%lf", ScaleValue.x);
	swprintf(text[1], L"%lf", ScaleValue.y);
	swprintf(text[2], L"%lf", ScaleValue.z);

	SetDlgItemText(hwnd, IDC_SCALEX, text[0]);
	SetDlgItemText(hwnd, IDC_SCALEY, text[1]);
	SetDlgItemText(hwnd, IDC_SCALEZ, text[2]);
}

float LoadObj::pick(XMVECTOR pickRayInWorldSpacePos,
	XMVECTOR pickRayInWorldSpaceDir)
{
	XMMATRIX worldSpace = XMLoadFloat4x4(&MeshWorld);

	//Loop through each triangle in the object
	for (int i = 0; i < indexPosArray.size() / 3; i++)
	{
		//Triangle's vertices V1, V2, V3
		XMVECTOR tri1V1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR tri1V2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR tri1V3 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

		//Temporary 3d floats for each vertex
		XMFLOAT3 tV1, tV2, tV3;

		//Get triangle 
		tV1 = vertPosArray[indexPosArray[(i * 3) + 0]];
		tV2 = vertPosArray[indexPosArray[(i * 3) + 1]];
		tV3 = vertPosArray[indexPosArray[(i * 3) + 2]];

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
	//return the max float value (near infinity) if an object was not picked
	return FLT_MAX;
}
bool LoadObj::PointInTriangle(XMVECTOR& triV1, XMVECTOR& triV2, XMVECTOR& triV3, XMVECTOR& point)
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
void LoadObj::SetSaveTranfrom()
{
	SavePositionValue = PositionValue;
	SaveRotationValue = RotationValue;
	SaveScaleValue = ScaleValue;
}
void LoadObj::ResetSaveTranfrom()
{
	SetPos(SavePositionValue.x, SavePositionValue.y, SavePositionValue.z);
	SetRotation(SaveRotationValue.x, SaveRotationValue.y, SaveRotationValue.z);
	SetScale(SaveScaleValue.x, SaveScaleValue.y, SaveScaleValue.z);
}