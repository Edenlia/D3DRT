#include "RenderTime.h"

RenderTime::RenderTime() : 
	m_secondsPerCount(0.0), 
	m_deltaTime(-1.0), 
	m_baseTime(0), 
	m_pausedTime(0), 
	m_stopTime(0), 
	m_prevTime(0), 
	m_currTime(0), 
	m_stopped(false)
{
	// Calculate the frequency of the performance counter
	// and store it in countsPerSec
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	m_secondsPerCount = 1.0 / (double)countsPerSec;
}

void RenderTime::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	// If we are resuming the timer from a stopped state
	if (m_stopped)
	{
		// Accumulate the paused time
		m_pausedTime += (startTime - m_stopTime);

		// Since we are starting the timer back up, the current
		// previous time is not valid, as it occurred while paused
		// So reset it to the current time
		m_prevTime = startTime;

		// No longer stopped
		m_stopTime = 0;
		m_stopped = false;
	}
}

void RenderTime::Stop()
{
	if (!m_stopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		m_stopTime = currTime;
		m_stopped = true;
	}
}

void RenderTime::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	m_baseTime = currTime; // set base time as current time
	m_prevTime = currTime; // set previous time as current time, because there is no previous time yet
	m_stopTime = 0;
	m_stopped = false;
}

void RenderTime::Tick()
{
	if (m_stopped)
	{
		m_deltaTime = 0.0;
		return;
	}

	// Get the time this frame
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	m_currTime = currTime;

	// Time difference between this frame and the previous
	m_deltaTime = (m_currTime - m_prevTime) * m_secondsPerCount;

	// Prepare for next frame
	m_prevTime = m_currTime;

	// Force nonnegative. The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then m_deltaTime can be negative
	if (m_deltaTime < 0.0)
	{
		m_deltaTime = 0.0;
	}
}

bool RenderTime::IsStopped() const
{
	return m_stopped;
}

float RenderTime::GetDeltaTime() const
{
	return (float)m_deltaTime;
}

float RenderTime::GetTotalTime() const
{
	if (m_stopped)
	{
		return (float)(((m_stopTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
	}
	else
	{
		return (float)(((m_currTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
	}
}

