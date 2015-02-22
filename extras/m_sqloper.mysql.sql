CREATE TABLE ircd_opers (
  id bigint(20) NOT NULL auto_increment,
  username text,
  password text,
  hostname text,
  type text,
  active tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY  (id)
) ENGINE=MyISAM;
