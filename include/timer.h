class InspTimer
{
 private:
	time_t trigger;
 public:
	virtual InspTimer(long secs_from_now) : trigger(time(NULL) + secs_from_now) { }
	virtual ~InspTimer() { }
	virtual time_t GetTimer()
	{
		return trigger;
	}
	virtual void Tick(time_t TIME) {}
};


