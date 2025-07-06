AUDIO_State :: struct
{
  device: SDL_AudioDeviceID;
  streams: [..] *SDL_AudioStream;
}

AUDIO_SoundType :: enum u32
{
  HIT;
}

AUDIO_PlaySound :: (sound_type: AUDIO_SoundType)
{
  if xx sound_type >= G.ast.sounds.count
  {
    log_error("Can't play missing sound %", sound_type);
    return;
  }

  sound := G.ast.sounds[sound_type];

  src_audio_spec := SDL_AudioSpec.{.SDL_AUDIO_S32LE, 2, 48000};
  stream := SDL_CreateAudioStream(*src_audio_spec, null);
  if stream == null
  {
    log_error("Failed to create audio stream");
    return;
  }

  SDL_BindAudioStream(G.audio.device, stream);
  SDL_PutAudioStreamData(stream, sound.data, xx sound.count);
  SDL_ResumeAudioStreamDevice(stream);
  SDL_FlushAudioStream(stream);

  array_add(*G.audio.streams, stream);
}

AUDIO_CloseFinishedStreams :: ()
{
  for G.audio.streams
  {
    if SDL_GetAudioStreamQueued(it) == 0
    {
      SDL_DestroyAudioStream(it);
      remove it;
    }
  }
}
