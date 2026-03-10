globallycoherent RWByteAddressBuffer Output : register(u0);

static const uint Message[] = { 'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!' };

void StoreByte(uint ByteOffset)
{
    uint AlignedOffset = (ByteOffset / 4) * 4;
    uint Shift = (ByteOffset % 4) * 8;
    uint PackedValue = Message[ByteOffset] << Shift;

    uint Dummy;
    Output.InterlockedOr(AlignedOffset, PackedValue, Dummy);
}

//this is the program entry point:
[Shader("node")]
[NodeLaunch("thread")]
void myProducer(
    [MaxRecords(1)]
    EmptyNodeOutput MyConsumer
)
{
    MyConsumer.ThreadIncrementOutputCount(1);
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(13, 1, 1)]
[NodeDispatchGrid(1, 1, 1)]
void MyConsumer(
    uint GroupID : SV_GroupID,
    uint ThreadInGroup : SV_GroupIndex
)
{
    StoreByte(ThreadInGroup);
}
