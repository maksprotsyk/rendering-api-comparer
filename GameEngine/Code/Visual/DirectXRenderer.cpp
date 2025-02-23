#include "DirectXRenderer.h"

#include <stdexcept>
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "tiny_obj_loader.h"

namespace Engine::Visual
{
	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::init(const Window& window)
	{
		createDeviceAndSwapChain(window.getHandle());
		createRenderTarget(window.getHandle());
		createShaders();
		createViewport(window.getHandle());

		if (!loadTexture(DEFAULT_TEXTURE))
		{
			// TODO: asserts, error handling
		}
		m_defaultMaterial.diffuseTextureId = DEFAULT_TEXTURE;
		m_defaultMaterial.diffuseColor = XMFLOAT3(0.1f, 0.1f, 0.1f);
		m_defaultMaterial.ambientColor = XMFLOAT3(0.5f, 0.5f, 0.5f);
		m_defaultMaterial.specularColor = XMFLOAT3(0.5f, 0.5f, 0.5f);
		m_defaultMaterial.shininess = 32.0f;
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::clearBackground(float r, float g, float b, float a)
	{
		// Clear the render target with a solid color
		float clearColor[] = {
			r, g, b, a
		}; // RGBA
		m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
		m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::draw(const IModelInstance& model, const Utils::Vector3& position, const Utils::Vector3& rotation, const Utils::Vector3& scale)
	{
		const auto& modelItr = m_models.find(model.GetId());
		if (modelItr == m_models.end())
		{
			// TODO: asserts, error handling
			return;
		}
		const ModelData& modelData = modelItr->second;

		XMMATRIX worldMatrix = getWorldMatrix(position, rotation, scale);

		// Bind the vertex and index buffers
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		m_deviceContext->IASetVertexBuffers(0, 1, modelData.vertexBuffer.GetAddressOf(), &stride, &offset);

		// Set the primitive topology
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		for (const auto& mesh : modelData.meshes)
		{
			// Update the constant buffer (world, view, projection matrices)
			ConstantBuffer cb;
			cb.worldMatrix = XMMatrixTranspose(worldMatrix);  // Transpose for HLSL
			cb.viewMatrix = XMMatrixTranspose(m_viewMatrix);
			cb.projectionMatrix = XMMatrixTranspose(m_projectionMatrix);

			const Material& material = mesh.materialId != -1 ? modelData.materials[mesh.materialId] : m_defaultMaterial;
			cb.ambientColor = material.ambientColor;
			cb.diffuseColor = material.diffuseColor;
			cb.specularColor = material.specularColor;
			cb.shininess = material.shininess;
			m_deviceContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

			// Bind the constant buffer and texture
			m_deviceContext->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
			m_deviceContext->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
			m_deviceContext->PSSetShaderResources(0, 1, getTexture(material.diffuseTextureId).GetAddressOf());
			m_deviceContext->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

			// Bind the index buffer for this sub-mesh
			m_deviceContext->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			// Draw the model
			m_deviceContext->DrawIndexed(mesh.indices.size(), 0, 0);
		}

	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::render()
	{
		// Present the frame
		m_swapChain->Present(0, 0);
	}

	////////////////////////////////////////////////////////////////////////

	// Create the Direct3D device, swap chain, and device context
	void DirectXRenderer::createDeviceAndSwapChain(HWND hwnd)
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = 1;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = hwnd;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = TRUE;

		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			featureLevels,
			ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION,
			&swapChainDesc,
			m_swapChain.GetAddressOf(),
			m_device.GetAddressOf(),
			nullptr,
			m_deviceContext.GetAddressOf()
		);

		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create DirectX device and swap chain.");
		}
	}

	////////////////////////////////////////////////////////////////////////

	// Create the render target
	void DirectXRenderer::createRenderTarget(HWND hwnd)
	{
		ComPtr<ID3D11Texture2D> backBuffer;
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());

		RECT rect;
		GetClientRect(hwnd, &rect);
		auto width = static_cast<float>(rect.right - rect.left);
		auto height = static_cast<float>(rect.bottom - rect.top);

		// Create the depth stencil buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc = {};
		depthStencilDesc.Width = static_cast<UINT>(width);
		depthStencilDesc.Height = static_cast<UINT>(height);
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ComPtr<ID3D11Texture2D> depthStencilBuffer;
		HRESULT hr = m_device->CreateTexture2D(&depthStencilDesc, nullptr, depthStencilBuffer.GetAddressOf());
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create depth stencil buffer.");
		}

		// Create the depth stencil view
		hr = m_device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, m_depthStencilView.GetAddressOf());
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create depth stencil view.");
		}

		// Set the render target and depth stencil
		m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	}

	////////////////////////////////////////////////////////////////////////

	// Create the shaders and input layout
	void DirectXRenderer::createShaders()
	{
		auto vsBytecode = Utils::loadBytesFromFile("VertexShader.cso");
		auto psBytecode = Utils::loadBytesFromFile("PixelShader.cso");

		// Create shaders
		m_device->CreateVertexShader(vsBytecode.data(), vsBytecode.size(), nullptr, m_vertexShader.GetAddressOf());
		m_device->CreatePixelShader(psBytecode.data(), psBytecode.size(), nullptr, m_pixelShader.GetAddressOf());
		m_deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		m_deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		// Create input layout
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		m_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBytecode.data(), vsBytecode.size(), m_inputLayout.GetAddressOf());
		m_deviceContext->IASetInputLayout(m_inputLayout.Get());

		// Create constant buffer
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(ConstantBuffer);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = 0;
		HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create constant buffer.");
		}

		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

		// Enable culling of backfacesss
		rasterizerDesc.CullMode = D3D11_CULL_BACK;  // Cull back-facing polygons
		rasterizerDesc.FillMode = D3D11_FILL_SOLID; // Render polygons as solid (not wireframe)
		rasterizerDesc.FrontCounterClockwise = FALSE; // Assume clockwise vertices are front-facing
		rasterizerDesc.DepthClipEnable = TRUE;  // Enable depth clipping

		// Create the rasterizer state
		ID3D11RasterizerState* rasterizerState;
		hr = m_device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create rasterizer state.");
		}

		// Bind the rasterizer state
		m_deviceContext->RSSetState(rasterizerState);

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		ComPtr<ID3D11DepthStencilState> depthStencilState;
		hr = m_device->CreateDepthStencilState(&depthStencilDesc, depthStencilState.GetAddressOf());
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create depth stencil state.");
		}

		// Set the depth stencil state
		m_deviceContext->OMSetDepthStencilState(depthStencilState.Get(), 1);
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::createBuffersForModel(ModelData& model)
	{
		// Create vertex buffer
		D3D11_BUFFER_DESC vertexBufferDesc = {};
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(Vertex) * model.vertices.size();
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexData = {};
		vertexData.pSysMem = model.vertices.data();
		m_device->CreateBuffer(&vertexBufferDesc, &vertexData, model.vertexBuffer.GetAddressOf());

		for (auto& subMesh: model.meshes)
		{
			// Create the index buffer for this sub-mesh
			D3D11_BUFFER_DESC indexBufferDesc = {};
			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.ByteWidth = sizeof(unsigned int) * subMesh.indices.size();  // Size of index buffer
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA indexData = {};
			indexData.pSysMem = subMesh.indices.data();  // Pointer to the index data

			HRESULT hr = m_device->CreateBuffer(&indexBufferDesc, &indexData, subMesh.indexBuffer.GetAddressOf());
			if (FAILED(hr)) 
			{
				throw std::runtime_error("Failed to create index buffer.");
			}
		}
	}

	////////////////////////////////////////////////////////////////////////

	const ComPtr<ID3D11ShaderResourceView>& DirectXRenderer::getTexture(const std::string& textureId) const
	{
		const auto& textureItr = m_textures.find(textureId);
		if (textureItr != m_textures.end())
		{
			return textureItr->second;
		}

		const auto& defaultTextureItr = m_textures.find(DEFAULT_TEXTURE);
		if (defaultTextureItr != m_textures.end())
		{
			return defaultTextureItr->second;
		}

		// TODO: asserts, error handling
		return nullptr;
	}

	////////////////////////////////////////////////////////////////////////

	bool DirectXRenderer::loadModel(const std::string& filename)
	{
		if (m_models.contains(filename))
		{
			return true;
		}

		ModelData modelData;
		if (!loadModelFromFile(modelData, filename))
		{
			return false;
		}
		createBuffersForModel(modelData);
		m_models.emplace(filename, std::move(modelData));
		return true;
	}

	////////////////////////////////////////////////////////////////////////

	bool DirectXRenderer::loadTexture(const std::string& filename)
	{
		if (m_textures.contains(filename))
		{
			return true;
		}

		ComPtr<ID3D11ShaderResourceView> texture;
		HRESULT hr = DirectX::CreateWICTextureFromFile(m_device.Get(), m_deviceContext.Get(), Utils::stringToWString(filename).c_str(), nullptr, texture.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}

		m_textures.emplace(filename, std::move(texture));
		return true;
	}

	////////////////////////////////////////////////////////////////////////

	bool DirectXRenderer::loadModelFromFile(ModelData& model, const std::string& filename)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::filesystem::path fullPath(filename);
		std::filesystem::path matDir = fullPath.parent_path();
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), matDir.string().c_str());
		if (!ret) 
		{
			return false;
		}

		for (const tinyobj::material_t& mat : materials)
		{
			Material matX;
			matX.ambientColor = XMFLOAT3(mat.ambient);
			matX.diffuseColor = XMFLOAT3(mat.diffuse);
			matX.shininess = mat.shininess;
			matX.specularColor = XMFLOAT3(mat.specular);
			if (!mat.diffuse_texname.empty())
			{
				std::string path = (matDir / mat.diffuse_texname).string();
				if (loadTexture(path))
				{
					matX.diffuseTextureId = path;
				}
				else
				{
					matX.diffuseTextureId = m_defaultMaterial.diffuseTextureId;
				}
			}
			else
			{
				matX.diffuseTextureId = m_defaultMaterial.diffuseTextureId;
			}
			model.materials.emplace_back(matX);
		}

		for (const auto& shape : shapes)
		{
			SubMesh mesh;
			for (const auto& index : shape.mesh.indices)
			{
				Vertex vertex;

				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				if (index.normal_index >= 0) {
					vertex.normal = {
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2]
					};
				}

				if (index.texcoord_index >= 0)
				{
					vertex.texCoord = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						attrib.texcoords[2 * index.texcoord_index + 1]
					};
				}

				model.vertices.emplace_back(vertex);
				mesh.indices.push_back(model.vertices.size() - 1);
			}

			mesh.materialId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
			model.meshes.emplace_back(std::move(mesh));
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::setCameraProperties(const Utils::Vector3& position, const Utils::Vector3& rotation)
	{
		XMVECTOR rotationQuaternion = XMQuaternionRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
    
		// Define the forward and up vectors
		XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    
		// Rotate forward and up vectors by the camera's rotation
		forward = XMVector3Rotate(forward, rotationQuaternion);
		up = XMVector3Rotate(up, rotationQuaternion);
    
		XMFLOAT3 positionX(position.x, position.y, position.z);
		// Calculate the target position based on forward direction
		XMVECTOR target = XMVectorAdd(XMLoadFloat3(&positionX), forward);
    
		// Create the view matrix using LookAt
		m_viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&positionX), target, up);
	}

	////////////////////////////////////////////////////////////////////////

	std::unique_ptr<IModelInstance> DirectXRenderer::createModelInstance(const std::string& filename)
	{
		return std::make_unique<ModelInstanceBase>(filename);
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::destroyModelInstance(IModelInstance& modelInstance)
	{
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::unloadTexture(const std::string& filename)
	{
		const auto& itr = m_textures.find(filename);
		if (itr == m_textures.end())
		{
			return;
		}

		ComPtr<ID3D11ShaderResourceView>& texture = itr->second;
		texture.Reset();

		m_textures.erase(itr);
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::unloadModel(const std::string& filename)
	{
		const auto& itr = m_models.find(filename);
		if (itr == m_models.end())
		{
			return;
		}

		ModelData& modelData = itr->second;
		for (SubMesh& mesh : modelData.meshes)
		{
			mesh.indexBuffer.Reset();
		}

		modelData.vertexBuffer.Reset();

		m_models.erase(itr);
		
	}

	////////////////////////////////////////////////////////////////////////

	void DirectXRenderer::cleanUp()
	{
		for (const std::string& modelId : Utils::getKeys(m_models))
		{
			unloadModel(modelId);
		}

		for (const std::string& textureId : Utils::getKeys(m_textures))
		{
			unloadTexture(textureId);
		}

		destroyComPtrSafe(m_samplerState);
		destroyComPtrSafe(m_constantBuffer);
		destroyComPtrSafe(m_vertexShader);
		destroyComPtrSafe(m_pixelShader);
		destroyComPtrSafe(m_inputLayout);
		destroyComPtrSafe(m_depthStencilView);
		destroyComPtrSafe(m_renderTargetView);
		destroyComPtrSafe(m_swapChain);
		destroyComPtrSafe(m_deviceContext);
		destroyComPtrSafe(m_device);
	}

	////////////////////////////////////////////////////////////////////////

	XMMATRIX DirectXRenderer::getWorldMatrix(const Utils::Vector3& position, const Utils::Vector3& rotation, const Utils::Vector3& scale)
	{
		XMMATRIX scalingMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);

		XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

		// Create a translation matrix from the position vector
		XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);

		// Combine the rotation and translation matrices to form the world matrix
		return XMMatrixMultiply(scalingMatrix, XMMatrixMultiply(rotationMatrix, translationMatrix));
	}

	////////////////////////////////////////////////////////////////////////

	// Set up the viewport
	void DirectXRenderer::createViewport(HWND hwnd)
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		auto width = static_cast<float>(rect.right - rect.left);
		auto height = static_cast<float>(rect.bottom - rect.top);

		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_deviceContext->RSSetViewports(1, &viewport);

		// Set up camera view and projection matrices
		m_viewMatrix = XMMatrixLookAtLH(XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		m_projectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / height, 0.1f, 500.0f);
	}

	////////////////////////////////////////////////////////////////////////

	XMFLOAT3 DirectXRenderer::computeFaceNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2) {
		// Calculate two edges of the triangle
		XMFLOAT3 edge1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
		XMFLOAT3 edge2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

		// Compute the cross product of edge1 and edge2 to get the face normal
		XMFLOAT3 normal = {
			edge1.y * edge2.z - edge1.z * edge2.y,
			edge1.z * edge2.x - edge1.x * edge2.z,
			edge1.x * edge2.y - edge1.y * edge2.x
		};

		// Normalize the normal vector
		float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
		normal.x /= length;
		normal.y /= length;
		normal.z /= length;

		return normal;
	}

	////////////////////////////////////////////////////////////////////////

}