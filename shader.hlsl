RWByteAddressBuffer Output : register(u0);

static const uint Message[] = {
	'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'
};

void StoreByte(uint ByteOffset)
{
	const uint AlignedOffset = (ByteOffset / 4) * 4;
	const uint Shift = (ByteOffset % 4) * 8;
	const uint PackedValue = Message[ByteOffset] << Shift;

	uint Dummy;
	Output.InterlockedOr(AlignedOffset, PackedValue, Dummy);
}

//this is the program entry point:
[Shader("node")]
[NodeLaunch("thread")]
[NodeIsProgramEntry]
[Numthreads(1, 1, 1)]
void MyProducer(
	[MaxRecords(1)] [NodeID("MyConsumer")] EmptyNodeOutput MyCons
)
{
	MyCons.ThreadIncrementOutputCount(1);
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(sizeof(Message) / sizeof(Message[0]), 1, 1)]
[NodeDispatchGrid(1, 1, 1)]
[NodeID("MyConsumer", 0)]
void MyConsumer(
	uint3 ThreadInGroup : SV_DispatchThreadID
)
{
	StoreByte(ThreadInGroup.x);
}
