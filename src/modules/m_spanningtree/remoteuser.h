#ifndef M_SPANNINGTREE_REMOTEUSER_H
#define M_SPANNINGTREE_REMOTEUSER_H
class CoreExport RemoteUser : public User
{
 public:
	TreeServer* srv;
	RemoteUser(const std::string& uid, TreeServer* Srv) :
		User(uid, Srv->GetName(), USERTYPE_REMOTE), srv(Srv)
	{
	}
	virtual void SendText(const std::string& line);
	virtual void DoWhois(User* src);
};

#endif
