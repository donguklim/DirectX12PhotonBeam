
#ifndef PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER
#define PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER

#define SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE 256


RWBuffer<uint> g_subBeamInstanceBuffer : register(u0, space0);


[numthreads(SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	g_subBeamInstanceBuffer[dispatchThreadID.x] = 0;
}

#endif
