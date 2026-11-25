#include <cassert>

#include <lexx/animation_timeline.h>

int main() {
	using lexx::graphics::AnimationClip;
	using lexx::graphics::AnimationTimeline;
	using lexx::graphics::PlaybackMode;
	AnimationClip loop{0U, 3U, 100U, PlaybackMode::Loop};
	AnimationTimeline t{loop};
	assert(t.valid());
	t.start(1000U);
	assert(t.frame_at(1000U) == 0U);
	assert(t.frame_at(1099U) == 0U);
	assert(t.frame_at(1100U) == 1U);
	assert(t.frame_at(1299U) == 2U);
	assert(t.frame_at(1300U) == 0U);
	t.pause(1150U);
	assert(t.frame_at(9000U) == 1U);
	t.resume(5000U);
	assert(t.frame_at(5050U) == 2U);
	t.stop();
	assert(t.frame_at(9000U) == 0U);

	AnimationTimeline ping{
	    AnimationClip{0U, 3U, 10U, PlaybackMode::PingPong}
    };
	ping.start(0U);
	assert(ping.frame_at(0U) == 0U);
	assert(ping.frame_at(10U) == 1U);
	assert(ping.frame_at(20U) == 2U);
	assert(ping.frame_at(30U) == 1U);
	assert(ping.frame_at(40U) == 0U);

	unsigned short const times[]{10, 20, 30};
	AnimationTimeline once{
	    AnimationClip{0U, 3U, 100U, PlaybackMode::Once, times}
    };
	once.start(0U);
	assert(once.frame_at(9U) == 0U);
	assert(once.frame_at(10U) == 1U);
	assert(once.frame_at(29U) == 1U);
	assert(once.frame_at(30U) == 2U);
	assert(once.frame_at(1000U) == 2U);

	AnimationTimeline wrapped{loop};
	wrapped.start(0xFFFFFFF0U);
	assert(wrapped.frame_at(0x00000054U) == 1U); // 100ms after wrap.
	assert(!AnimationTimeline(AnimationClip{0U, 0U, 10U}).valid());
	assert(!AnimationTimeline(AnimationClip{0U, 2U, 0U}).valid());
	unsigned short const invalidTimes[]{10U, 0U};
	assert(!AnimationTimeline(AnimationClip{0U, 2U, 10U, PlaybackMode::Loop, invalidTimes}).valid());
}
