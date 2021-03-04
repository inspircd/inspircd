CREATE TABLE ircd_opers (
  id bigint(20) NOT NULL auto_increment,
  name text NOT NULL,
  password text NOT NULL,
  hash text,
  host text NOT NULL,
  type text NOT NULL,
  fingerprint text,
  autologin tinyint(1) NOT NULL DEFAULT 0,
  active tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY  (id)
) ENGINE=InnoDB;
