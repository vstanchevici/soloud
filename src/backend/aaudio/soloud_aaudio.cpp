#include "soloud.h"
#include <aaudio/AAudio.h>

struct AAudioStaticData {
    AAudioStream*   stream;
    SoLoud::Soloud* soloud;
};

#if !defined(WITH_AAUDIO)

namespace SoLoud
{
    result aaudio_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer,  unsigned int aChannels)
	{
		return NOT_IMPLEMENTED;
	}
};

#else
namespace SoLoud {

    aaudio_data_callback_result_t soloud_aaudio_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames)
    {
        AAudioStaticData *data = (AAudioStaticData*)userData;
        float *floatData = (float*)audioData;

        // Ask SoLoud to fill the AAudio buffer
        // SoLoud's mixer outputs Float by default, which AAudio loves.
        data->soloud->mix(floatData, numFrames);

        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    void aaudio_deinit(Soloud *aSoloud)
    {
        if (aSoloud->mBackendData == nullptr)
            return;

        AAudioStaticData *aaudioData = (AAudioStaticData*)aSoloud->mBackendData;

        if (aaudioData->stream != nullptr) {
            // Stop the stream (stops the callback from being called)
            AAudioStream_requestStop(aaudioData->stream);

            // Close the stream (releases hardware resources)
            AAudioStream_close(aaudioData->stream);
            aaudioData->stream = nullptr;
        }

        // Delete the data holder
        delete aaudioData;
        aSoloud->mBackendData = nullptr;
    }

    result aaudio_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer,  unsigned int aChannels)
    {
        AAudioStaticData *aaudioData = new AAudioStaticData;
        aaudioData->soloud = aSoloud;

        AAudioStreamBuilder *builder;
        AAudio_createStreamBuilder(&builder);

        // Configure for Low Latency
        AAudioStreamBuilder_setSampleRate(builder, aSamplerate);
        AAudioStreamBuilder_setChannelCount(builder, aChannels);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setDataCallback(builder, soloud_aaudio_callback, aaudioData);

        // Try Exclusive first
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
        aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &aaudioData->stream);

        if (result != AAUDIO_OK) {
            // Fallback to Shared
            AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
            result = AAudioStreamBuilder_openStream(builder, &aaudioData->stream);
        }

        if (result != AAUDIO_OK) {
            delete aaudioData;
            AAudioStreamBuilder_delete(builder);
            return UNKNOWN_ERROR;
        }

        AAudioStream_requestStart(aaudioData->stream);

        // Store our data in SoLoud so we can clean it up later
        aSoloud->mBackendData = aaudioData;
        aSoloud->mBackendCleanupFunc = aaudio_deinit;
        aSoloud->mBackendString = "AAudio";

        // Inform SoLoud about the final hardware settings
        int32_t framesPerBurst = AAudioStream_getFramesPerBurst(aaudioData->stream);
        unsigned int bufferSize = (aBuffer == 0) ? framesPerBurst : aBuffer;
        aSoloud->postinit_internal(aSamplerate, bufferSize, aFlags, aChannels);

        AAudioStreamBuilder_delete(builder);

        return SO_NO_ERROR;
    }
}
#endif
