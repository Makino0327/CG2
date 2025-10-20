#pragma once
#include <wrl/client.h> // ComPtr 用
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

using Microsoft::WRL::ComPtr;

class ResourceObject
{
public:
	ResourceObject(ID3D12Resource* resource)
		:resource_(resource)
	{ }
	~ResourceObject()
	{
		if (resource_)
		{
			resource_->Release();
		}
	}

	ID3D12Resource* Get() {return resource_;}

private:
	ID3D12Resource* resource_;
};

