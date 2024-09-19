#include "ragdollpch.h"
#include "Renderer.h"

#include "DirectXDevice.h"

void Renderer::CreateDevice(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm)
{
	Device = DirectXDevice::Create({}, win, fm);
}

int32_t Renderer::AddTextureToTable(nvrhi::TextureHandle tex)
{
	Device->m_NvrhiDevice->writeDescriptorTable(DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(TextureCount++, tex));
	return TextureCount - 1;
}
