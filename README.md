# DirectSound buffer position retimer

I have an old game running on Windows XP that synchronises to music. It
constantly compares the music's play position (using
`IDirectSoundBuffer::GetCurrentPosition`) to its expected location, and seeks
the music position if it is out of sync.

However, on Windows 10 (and probably anything newer than Vista), the DirectSound
API doesn't update the buffer playback position very often. Because of this, the
game retimes the audio every frame, causing crackling and distortion.

Interestingly, adding the `DSBCAPS_TRUEPLAYPOSITION` flag to
`IDirectSound8::CreateSoundBuffer`'s flags does nothing to fix the timing.

This hook library simulates a perfectly timed music file using
`QueryPerformanceCounter`, which fixes the timing issues and makes the game run
as expected.

The COM API is hooked using capnhook's com-proxy. Built using Visual Studio
2019.

## Install

Grab DSOUND.dll from "Releases" and place it next to your malfunctioning game's
exe. If you're lucky, it will work perfectly and fix all your problems.
