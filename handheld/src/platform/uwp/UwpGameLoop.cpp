#include "pch.h"
#include "UwpGameLoop.h"

void UwpGameLoop::setTickHandler(TickHandler handler)
{
	m_tickHandler = std::move(handler);
}

void UwpGameLoop::setRenderHandler(RenderHandler handler)
{
	m_renderHandler = std::move(handler);
}

void UwpGameLoop::reset()
{
	m_accumulator = 0.0;
	m_tickCount = 0;
}

void UwpGameLoop::advance(double elapsedSeconds)
{
	// A suspended/resumed UWP app can report a very large frame gap.  Do not
	// turn that gap into hundreds of simulation ticks in one UI callback.
	if (elapsedSeconds < 0.0)
	{
		elapsedSeconds = 0.0;
	}
	else if (elapsedSeconds > 0.25)
	{
		elapsedSeconds = 0.25;
	}
	m_accumulator += elapsedSeconds;

	unsigned int ticksThisFrame = 0;
	while (m_accumulator >= TickIntervalSeconds && ticksThisFrame < 5)
	{
		m_accumulator -= TickIntervalSeconds;
		++m_tickCount;
		++ticksThisFrame;
		if (m_tickHandler)
		{
			m_tickHandler(m_tickCount);
		}
	}

	// Keep the scheduler bounded if a frame was stalled for a long time.
	if (ticksThisFrame == 5 && m_accumulator >= TickIntervalSeconds)
	{
		m_accumulator = 0.0;
	}

	if (m_renderHandler)
	{
		m_renderHandler(static_cast<float>(m_accumulator / TickIntervalSeconds));
	}
}
