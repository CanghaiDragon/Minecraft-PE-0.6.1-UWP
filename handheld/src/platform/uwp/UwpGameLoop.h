#pragma once

#include <cstdint>
#include <functional>

// Platform-neutral fixed-step scheduler for the MCPE engine.  Rendering may
// arrive at a variable rate on UWP, but simulation remains at the original
// 20 ticks per second.  The callbacks are intentionally independent of
// Minecraft so this class can be connected after the core is made UWP-safe.
class UwpGameLoop final
{
public:
	using TickHandler = std::function<void(std::uint64_t)>;
	using RenderHandler = std::function<void(float)>;

	void setTickHandler(TickHandler handler);
	void setRenderHandler(RenderHandler handler);
	void reset();
	void advance(double elapsedSeconds);

	std::uint64_t tickCount() const { return m_tickCount; }
	static constexpr double TickIntervalSeconds = 0.05;

private:
	double m_accumulator = 0.0;
	std::uint64_t m_tickCount = 0;
	TickHandler m_tickHandler;
	RenderHandler m_renderHandler;
};
