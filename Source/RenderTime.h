#ifndef _RENDER_TIME_H_
#define _RENDER_TIME_H_

#include <Windows.h>

class RenderTime
{
public:
	RenderTime();

	void Start();
	void Stop();
	void Reset();
	void Tick();

	bool IsStopped() const;

	float GetDeltaTime() const;
	float GetTotalTime() const;

private:
	double m_secondsPerCount;	// 1 / frequency
	double m_deltaTime; // time between frames

	__int64 m_baseTime;		// time after reset
	__int64 m_pausedTime;	// total paused time
	__int64 m_stopTime;		// time when timer is stopped
	__int64 m_prevTime;		// time in previous frame
	__int64 m_currTime;		// time in current frame

	bool m_stopped;			// true if timer stopped
};

#endif // !RENDER_TIME
