#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <stdio.h>
#include <stdbool.h>

#pragma comment(linker, "/DEFAULTLIB:D3d12.lib")
#pragma comment(linker, "/DEFAULTLIB:DXGI.lib")
#pragma comment(linker, "/DEFAULTLIB:dxguid.lib")

__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
__declspec(dllexport) UINT D3D12SDKVersion = 613;
__declspec(dllexport) char* D3D12SDKPath = ".\\D3D12\\";

HANDLE ConsoleHandle;
ID3D12Device10* Device;

inline void THROW_ON_FAIL_IMPL(HRESULT hr, int line)
{
	if (hr == 0x887A0005)//device removed
	{
		THROW_ON_FAIL_IMPL(ID3D12Device10_GetDeviceRemovedReason(Device), line);
	}

	if (FAILED(hr))
	{
		LPWSTR messageBuffer;
		DWORD formattedErrorLength = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPWSTR)&messageBuffer,
			0,
			NULL
		);

		if (formattedErrorLength == 0)
			WriteConsoleA(ConsoleHandle, "an error occured, unable to retrieve error message\n", 51, NULL, NULL);
		else
		{
			WriteConsoleA(ConsoleHandle, "an error occured: ", 18, NULL, NULL);
			WriteConsoleW(ConsoleHandle, messageBuffer, formattedErrorLength, NULL, NULL);
			WriteConsoleA(ConsoleHandle, "\n", 1, NULL, NULL);
			LocalFree(messageBuffer);
		}

		char buffer[50];
		int stringlength = _snprintf_s(buffer, 50, _TRUNCATE, "error code: 0x%X\nlocation:line %i\n", hr, line);
		WriteConsoleA(ConsoleHandle, buffer, stringlength, NULL, NULL);

		RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, NULL);
	}
}

#define THROW_ON_FAIL(x) THROW_ON_FAIL_IMPL(x, __LINE__)

#define THROW_ON_FALSE(x) if((x) == FALSE) THROW_ON_FAIL(HRESULT_FROM_WIN32(GetLastError()))

#define VALIDATE_HANDLE(x) if((x) == NULL || (x) == INVALID_HANDLE_VALUE) THROW_ON_FAIL(HRESULT_FROM_WIN32(GetLastError()))

static const SIZE_T UAV_SIZE = 1024;

static const wchar_t* const PROGRAM_NAME = L"Hello World";
static const bool bWarp = true;

int main()
{
	ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

#ifdef _DEBUG
	ID3D12Debug6* DebugController;

	{
		ID3D12Debug* DebugControllerV1;
		THROW_ON_FAIL(D3D12GetDebugInterface(&IID_ID3D12Debug, &DebugControllerV1));
		THROW_ON_FAIL(ID3D12Debug_QueryInterface(DebugControllerV1, &IID_ID3D12Debug6, &DebugController));
		ID3D12Debug_Release(DebugControllerV1);
	}

	ID3D12Debug6_EnableDebugLayer(DebugController);
	ID3D12Debug6_SetEnableSynchronizedCommandQueueValidation(DebugController, TRUE);
	ID3D12Debug6_SetGPUBasedValidationFlags(DebugController, D3D12_GPU_BASED_VALIDATION_FLAGS_DISABLE_STATE_TRACKING);
	ID3D12Debug6_SetEnableGPUBasedValidation(DebugController, TRUE);
#endif

	IDXGIFactory6* Factory;
	THROW_ON_FAIL(CreateDXGIFactory2(0, &IID_IDXGIFactory6, &Factory));

	{
		IDXGIAdapter1* Adapter;

		if (bWarp)
		{
			THROW_ON_FAIL(IDXGIFactory6_EnumWarpAdapter(Factory, &IID_IDXGIAdapter1, &Adapter));
		}
		else
		{
			THROW_ON_FAIL(IDXGIFactory6_EnumAdapterByGpuPreference(Factory, 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, &Adapter));
		}

		THROW_ON_FAIL(D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_12_1, &IID_ID3D12Device, &Device));
		THROW_ON_FAIL(IDXGIAdapter1_Release(Adapter));
	}

	THROW_ON_FAIL(IDXGIFactory6_Release(Factory));

#ifdef _DEBUG
	ID3D12InfoQueue* InfoQueue;
	THROW_ON_FAIL(ID3D12Device10_QueryInterface(Device, &IID_ID3D12InfoQueue, &InfoQueue));

	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
	THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#endif

	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS21 Options21 = { 0 };
		ID3D12Device10_CheckFeatureSupport(Device, D3D12_FEATURE_D3D12_OPTIONS21, &Options21, sizeof(Options21));
		if (Options21.WorkGraphsTier == D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED)
			ExitProcess(0);
	}

	ID3D12RootSignature* GlobalRootSignature;

	{
		D3D12_ROOT_PARAMETER RootSignatureUAV = { 0 };
		RootSignatureUAV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		RootSignatureUAV.Descriptor.ShaderRegister = 0;
		RootSignatureUAV.Descriptor.RegisterSpace = 0;
		RootSignatureUAV.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDescription = { 0 };
		RootSignatureDescription.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		RootSignatureDescription.Desc_1_1.NumParameters = 1;
		RootSignatureDescription.Desc_1_1.pParameters = &RootSignatureUAV;
		RootSignatureDescription.Desc_1_1.NumStaticSamplers = 0;
		RootSignatureDescription.Desc_1_1.pStaticSamplers = NULL;
		RootSignatureDescription.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ID3D10Blob* RootSig;
		THROW_ON_FAIL(D3D12SerializeVersionedRootSignature(&RootSignatureDescription, &RootSig, NULL));
		THROW_ON_FAIL(ID3D12Device10_CreateRootSignature(Device, 0, ID3D10Blob_GetBufferPointer(RootSig), ID3D10Blob_GetBufferSize(RootSig), &IID_ID3D12RootSignature, &GlobalRootSignature));
		THROW_ON_FAIL(ID3D10Blob_Release(RootSig));
	}

	ID3D12StateObject* StateObject;

	{
		HANDLE ShaderFile = CreateFileW(L"shader.cso", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		VALIDATE_HANDLE(ShaderFile);

		LONGLONG ShaderFileSize;
		THROW_ON_FALSE(GetFileSizeEx(ShaderFile, &ShaderFileSize));

		HANDLE ShaderFileMap = CreateFileMappingW(ShaderFile, NULL, PAGE_READONLY, 0, 0, NULL);
		VALIDATE_HANDLE(ShaderFileMap);

		const void* ShaderFilePtr = MapViewOfFile(ShaderFileMap, FILE_MAP_READ, 0, 0, 0);

		D3D12_DXIL_LIBRARY_DESC LibraryDesc = { 0 };
		LibraryDesc.DXILLibrary.pShaderBytecode = ShaderFilePtr;
		LibraryDesc.DXILLibrary.BytecodeLength = ShaderFileSize;

		D3D12_WORK_GRAPH_DESC WorkGraphDesc = { 0 };
		WorkGraphDesc.ProgramName = PROGRAM_NAME;
		WorkGraphDesc.Flags = D3D12_WORK_GRAPH_FLAG_INCLUDE_ALL_AVAILABLE_NODES;

		D3D12_STATE_SUBOBJECT StateSubojects[3] = { 0 };
		StateSubojects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		StateSubojects[0].pDesc = &GlobalRootSignature;

		StateSubojects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		StateSubojects[1].pDesc = &LibraryDesc;

		StateSubojects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH;
		StateSubojects[2].pDesc = &WorkGraphDesc;

		D3D12_STATE_OBJECT_DESC StateObjectDesc = { 0 };
		StateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE;
		StateObjectDesc.NumSubobjects = ARRAYSIZE(StateSubojects);
		StateObjectDesc.pSubobjects = StateSubojects;

		THROW_ON_FAIL(ID3D12Device10_CreateStateObject(Device, &StateObjectDesc, &IID_ID3D12StateObject, &StateObject));

		THROW_ON_FALSE(UnmapViewOfFile(ShaderFilePtr));
		THROW_ON_FALSE(CloseHandle(ShaderFileMap));
		THROW_ON_FALSE(CloseHandle(ShaderFile));
	}

	ID3D12Resource* BackingMemoryResource = NULL;

	D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS MemoryRequirements = { 0 };

	{
		ID3D12WorkGraphProperties* WorkGraphProperties;
		THROW_ON_FAIL(ID3D12StateObject_QueryInterface(StateObject, &IID_ID3D12WorkGraphProperties, &WorkGraphProperties));

		UINT WGIndex = ID3D12WorkGraphProperties_GetWorkGraphIndex(WorkGraphProperties, PROGRAM_NAME);
		ID3D12WorkGraphProperties_GetWorkGraphMemoryRequirements(WorkGraphProperties, WGIndex, &MemoryRequirements);

		THROW_ON_FAIL(ID3D12WorkGraphProperties_Release(WorkGraphProperties));
	}
	
	if (MemoryRequirements.MaxSizeInBytes > 0)
	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC1 ResourceDesc = { 0 };
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Width = MemoryRequirements.MaxSizeInBytes;
		ResourceDesc.Height = 1;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		THROW_ON_FAIL(ID3D12Device10_CreateCommittedResource3(
			Device,
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&ResourceDesc,
			D3D12_BARRIER_LAYOUT_UNDEFINED,
			NULL,
			NULL,
			0,
			NULL,
			&IID_ID3D12Resource,
			&BackingMemoryResource));
	}

	ID3D12CommandQueue* CommandQueue;

	{
		D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = { 0 };
		CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ID3D12Device10_CreateCommandQueue(Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &CommandQueue);
	}

	ID3D12CommandAllocator* CommandAllocator;
	THROW_ON_FAIL(ID3D12Device10_CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_COMPUTE, &IID_ID3D12CommandAllocator, &CommandAllocator));

	ID3D12GraphicsCommandList10* CommandList;
	THROW_ON_FAIL(ID3D12Device10_CreateCommandList(Device, 0, D3D12_COMMAND_LIST_TYPE_COMPUTE, CommandAllocator, NULL, &IID_ID3D12GraphicsCommandList10, &CommandList));

	ID3D12Fence* Fence;
	THROW_ON_FAIL(ID3D12Device10_CreateFence(Device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &Fence));

	ID3D12Resource* UAVBuffer;

	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC1 ResourceDesc = { 0 };
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Width = UAV_SIZE;
		ResourceDesc.Height = 1;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		THROW_ON_FAIL(ID3D12Device10_CreateCommittedResource3(
			Device,
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&ResourceDesc,
			D3D12_BARRIER_LAYOUT_UNDEFINED,
			NULL,
			NULL,
			0,
			NULL,
			&IID_ID3D12Resource,
			&UAVBuffer));
	}

	ID3D12Resource* ReadbackBuffer;

	{
		D3D12_HEAP_PROPERTIES HeapProperties = { 0 };
		HeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
		HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC1 ResourceDesc = { 0 };
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Width = UAV_SIZE;
		ResourceDesc.Height = 1;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		THROW_ON_FAIL(ID3D12Device10_CreateCommittedResource3(
			Device,
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&ResourceDesc,
			D3D12_BARRIER_LAYOUT_UNDEFINED,
			NULL,
			NULL,
			0,
			NULL,
			&IID_ID3D12Resource,
			&ReadbackBuffer));
	}

	ID3D12GraphicsCommandList10_SetComputeRootSignature(CommandList, GlobalRootSignature);
	ID3D12GraphicsCommandList10_SetComputeRootUnorderedAccessView(CommandList, 0, ID3D12Resource_GetGPUVirtualAddress(UAVBuffer));

	{
		D3D12_SET_PROGRAM_DESC SetProgramDesc = { 0 };
		SetProgramDesc.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;

		{
			ID3D12StateObjectProperties1* StateObjectProperties;
			THROW_ON_FAIL(ID3D12StateObject_QueryInterface(StateObject, &IID_ID3D12StateObjectProperties1, &StateObjectProperties));
			ID3D12StateObjectProperties1_GetProgramIdentifier(StateObjectProperties, &SetProgramDesc.WorkGraph.ProgramIdentifier, PROGRAM_NAME);
			THROW_ON_FAIL(ID3D12StateObjectProperties1_Release(StateObjectProperties));
		}

		SetProgramDesc.WorkGraph.Flags = D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE;
		if (BackingMemoryResource)
		{
			SetProgramDesc.WorkGraph.BackingMemory.StartAddress = ID3D12Resource_GetGPUVirtualAddress(BackingMemoryResource);
			SetProgramDesc.WorkGraph.BackingMemory.SizeInBytes = MemoryRequirements.MaxSizeInBytes;
		}

		ID3D12GraphicsCommandList10_SetProgram(CommandList, &SetProgramDesc);
	}

	{
		D3D12_DISPATCH_GRAPH_DESC DispatchGraphDesc = { 0 };
		DispatchGraphDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
		DispatchGraphDesc.NodeCPUInput.EntrypointIndex = 0;
		DispatchGraphDesc.NodeCPUInput.NumRecords = 1;
		ID3D12GraphicsCommandList10_DispatchGraph(CommandList, &DispatchGraphDesc);
	}

	{
		D3D12_BUFFER_BARRIER BufferBarrier = { 0 };
		BufferBarrier.SyncBefore = D3D12_BARRIER_SYNC_ALL;
		BufferBarrier.SyncAfter = D3D12_BARRIER_SYNC_COPY;
		BufferBarrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		BufferBarrier.AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE;
		BufferBarrier.pResource = UAVBuffer;
		BufferBarrier.Offset = 0;
		BufferBarrier.Size = UAV_SIZE;

		D3D12_BARRIER_GROUP ResourceBarrier = { 0 };
		ResourceBarrier.Type = D3D12_BARRIER_TYPE_BUFFER;
		ResourceBarrier.NumBarriers = 1;
		ResourceBarrier.pBufferBarriers = &BufferBarrier;
		ID3D12GraphicsCommandList10_Barrier(CommandList, 1, &ResourceBarrier);
	}
			
	ID3D12GraphicsCommandList10_CopyResource(CommandList, ReadbackBuffer, UAVBuffer);

	THROW_ON_FAIL(ID3D12GraphicsCommandList10_Close(CommandList));

	ID3D12CommandQueue_ExecuteCommandLists(CommandQueue, 1, &CommandList);
	THROW_ON_FAIL(ID3D12CommandQueue_Signal(CommandQueue, Fence, 1));

	HANDLE CommandListFinished = CreateEventW(NULL, FALSE, FALSE, NULL);
	VALIDATE_HANDLE(CommandListFinished);

	THROW_ON_FAIL(ID3D12Fence_SetEventOnCompletion(Fence, 1, CommandListFinished));
	THROW_ON_FALSE(WaitForSingleObject(CommandListFinished, INFINITE) == WAIT_OBJECT_0);
	THROW_ON_FALSE(CloseHandle(CommandListFinished));

	THROW_ON_FAIL(ID3D12CommandAllocator_Reset(CommandAllocator));
	THROW_ON_FAIL(ID3D12GraphicsCommandList10_Reset(CommandList, CommandAllocator, NULL));

	const char* pOutput;
	THROW_ON_FAIL(ID3D12Resource_Map(ReadbackBuffer, 0, NULL, &pOutput));
	printf("SUCCESS: Output was \"%s\"\nPress any key to terminate...\n", pOutput);
	ID3D12Resource_Unmap(ReadbackBuffer, 0, NULL);
	
	THROW_ON_FAIL(ID3D12Resource_Release(ReadbackBuffer));
	THROW_ON_FAIL(ID3D12Resource_Release(UAVBuffer));
	THROW_ON_FAIL(ID3D12Fence_Release(Fence));
	THROW_ON_FAIL(ID3D12GraphicsCommandList10_Release(CommandList));
	THROW_ON_FAIL(ID3D12CommandAllocator_Release(CommandAllocator));
	THROW_ON_FAIL(ID3D12CommandQueue_Release(CommandQueue));
	if(BackingMemoryResource)
		THROW_ON_FAIL(ID3D12Resource_Release(BackingMemoryResource));
	THROW_ON_FAIL(ID3D12StateObject_Release(StateObject));
	THROW_ON_FAIL(ID3D12RootSignature_Release(GlobalRootSignature));

#ifdef _DEBUG
	THROW_ON_FAIL(ID3D12InfoQueue_Release(InfoQueue));
	THROW_ON_FAIL(ID3D12Debug6_Release(DebugController));
#endif

	THROW_ON_FAIL(ID3D12Device10_Release(Device));

#ifdef _DEBUG
	{
		IDXGIDebug1* DxgiDebug;
		THROW_ON_FAIL(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, &DxgiDebug));
		THROW_ON_FAIL(IDXGIDebug1_ReportLiveObjects(DxgiDebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}
#endif

	return 0;
}
