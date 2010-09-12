
class CoreExport RemoteUser : public User
{
 public:
	TreeServer* srv;
	RemoteUser(const std::string& uid, TreeServer* Srv) :
		User(uid, Srv->GetName(), USERTYPE_REMOTE), srv(Srv)
	{
	}
	virtual void SendText(const std::string& line);
};


