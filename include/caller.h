/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CALLER__H__
#define __CALLER__H__

template <typename ReturnType> class CoreExport HandlerBase0
{
 public:
	virtual ReturnType Call() = 0;
	virtual ~HandlerBase0() { }
};

template <typename ReturnType, typename Param1> class CoreExport HandlerBase1
{
 public:
	virtual ReturnType Call(Param1) = 0;
	virtual ~HandlerBase1() { }
};

template <typename ReturnType, typename Param1, typename Param2> class CoreExport HandlerBase2
{
 public:
	virtual ReturnType Call(Param1, Param2) = 0;
	virtual ~HandlerBase2() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3> class CoreExport HandlerBase3
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3) = 0;
	virtual ~HandlerBase3() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4> class CoreExport HandlerBase4
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3, Param4) = 0;
	virtual ~HandlerBase4() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5> class CoreExport HandlerBase5
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3, Param4, Param5) = 0;
	virtual ~HandlerBase5() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6> class CoreExport HandlerBase6
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3, Param4, Param5, Param6) = 0;
	virtual ~HandlerBase6() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6, typename Param7> class CoreExport HandlerBase7
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3, Param4, Param5, Param6, Param7) = 0;
	virtual ~HandlerBase7() { }
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6, typename Param7, typename Param8> class CoreExport HandlerBase8
{
 public:
	virtual ReturnType Call(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8) = 0;
	virtual ~HandlerBase8() { }
};

template <typename HandlerType> class CoreExport caller
{
 public:
	HandlerType* target;

	caller(HandlerType* initial)
	: target(initial)
	{ }

	virtual ~caller() { }

	caller& operator=(HandlerType* newtarget)
	{
		target = newtarget;
		return *this;
	}
};

template <typename ReturnType> class CoreExport caller0 : public caller< HandlerBase0<ReturnType> >
{
 public:
	caller0(HandlerBase0<ReturnType>* initial)
	: caller< HandlerBase0<ReturnType> >::caller(initial)
	{ }

	virtual ReturnType operator() ()
	{
		return this->target->Call();
	}
};

template <typename ReturnType, typename Param1> class CoreExport caller1 : public caller< HandlerBase1<ReturnType, Param1> >
{
 public:
	caller1(HandlerBase1<ReturnType, Param1>* initial)
	: caller< HandlerBase1<ReturnType, Param1> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1)
	{
		return this->target->Call(param1);
	}
};

template <typename ReturnType, typename Param1, typename Param2> class CoreExport caller2 : public caller< HandlerBase2<ReturnType, Param1, Param2> >
{
 public:
	caller2(HandlerBase2<ReturnType, Param1, Param2>* initial)
	: caller< HandlerBase2<ReturnType, Param1, Param2> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2)
	{
		return this->target->Call(param1, param2);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3> class CoreExport caller3 : public caller< HandlerBase3<ReturnType, Param1, Param2, Param3> >
{
 public:
	caller3(HandlerBase3<ReturnType, Param1, Param2, Param3>* initial)
	: caller< HandlerBase3<ReturnType, Param1, Param2, Param3> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3)
	{
		return this->target->Call(param1, param2, param3);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4> class CoreExport caller4 : public caller< HandlerBase4<ReturnType, Param1, Param2, Param3, Param4> >
{
 public:
	caller4(HandlerBase4<ReturnType, Param1, Param2, Param3, Param4>* initial)
	: caller< HandlerBase4<ReturnType, Param1, Param2, Param3, Param4> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3, Param4 param4)
	{
		return this->target->Call(param1, param2, param3, param4);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5> class CoreExport caller5 : public caller< HandlerBase5<ReturnType, Param1, Param2, Param3, Param4, Param5> >
{
 public:
	caller5(HandlerBase5<ReturnType, Param1, Param2, Param3, Param4, Param5>* initial)
	: caller< HandlerBase5<ReturnType, Param1, Param2, Param3, Param4, Param5> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3, Param4 param4, Param5 param5)
	{
		return this->target->Call(param1, param2, param3, param4, param5);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6> class CoreExport caller6 : public caller< HandlerBase6<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6> >
{
 public:
	caller6(HandlerBase6<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6>* initial)
	: caller< HandlerBase6<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3, Param4 param4, Param5 param5, Param6 param6)
	{
		return this->target->Call(param1, param2, param3, param4, param5, param6);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6, typename Param7> class CoreExport caller7 : public caller< HandlerBase7<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7> >
{
 public:
	caller7(HandlerBase7<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7>* initial)
	: caller< HandlerBase7<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3, Param4 param4, Param5 param5, Param6 param6, Param7 param7)
	{
		return this->target->Call(param1, param2, param3, param4, param5, param6, param7);
	}
};

template <typename ReturnType, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5, typename Param6, typename Param7, typename Param8> class CoreExport caller8 : public caller< HandlerBase8<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8> >
{
 public:
	caller8(HandlerBase8<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8>* initial)
	: caller< HandlerBase8<ReturnType, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8> >::caller(initial)
	{ }

	virtual ReturnType operator() (Param1 param1, Param2 param2, Param3 param3, Param4 param4, Param5 param5, Param6 param6, Param7 param7, Param8 param8)
	{
		return this->target->Call(param1, param2, param3, param4, param5, param6, param7, param8);
	}
};

#endif

