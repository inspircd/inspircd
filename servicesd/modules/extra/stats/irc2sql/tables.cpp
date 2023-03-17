/*
 *
 * (C) 2013-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "irc2sql.h"

void IRC2SQL::CheckTables() {
    Anope::string geoquery("");

    if (firstrun) {
        /*
         * reset some tables to make sure they are really empty
         */
        this->sql->RunQuery("TRUNCATE TABLE " + prefix + "user");
        this->sql->RunQuery("TRUNCATE TABLE " + prefix + "chan");
        this->sql->RunQuery("TRUNCATE TABLE " + prefix + "ison");
        this->sql->RunQuery("UPDATE `" + prefix +
                            "server` SET currentusers=0, online='N'");
    }

    this->GetTables();

    if (GeoIPDB.equals_ci("country")) {
        if (!this->HasTable(prefix + "geoip_country")) {
            query = "CREATE TABLE `" + prefix + "geoip_country` ("
                    "`start` INT UNSIGNED NOT NULL,"
                    "`end` INT UNSIGNED NOT NULL,"
                    "`countrycode` varchar(2),"
                    "`countryname` varchar(50),"
                    "PRIMARY KEY `end` (`end`),"
                    "KEY `start` (`start`)"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
            this->RunQuery(query);
        }
    } else if (GeoIPDB.equals_ci("city")) {
        if (!this->HasTable(prefix + "geoip_city_blocks")) {
            query = "CREATE TABLE `" + prefix + "geoip_city_blocks` ("
                    "`start` INT UNSIGNED NOT NULL,"
                    "`end` INT UNSIGNED NOT NULL,"
                    "`locId` INT UNSIGNED NOT NULL,"
                    "PRIMARY KEY `end` (`end`),"
                    "KEY `start` (`start`)"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
            this->RunQuery(query);
        }
        if (!this->HasTable(prefix + "geoip_city_location")) {
            query = "CREATE TABLE `" + prefix + "geoip_city_location` ("
                    "`locId` INT UNSIGNED NOT NULL,"
                    "`country` CHAR(2) NOT NULL,"
                    "`region` CHAR(2) NOT NULL,"
                    "`city` VARCHAR(50),"
                    "`latitude` FLOAT,"
                    "`longitude` FLOAT,"
                    "`areaCode` INT,"
                    "PRIMARY KEY (`locId`)"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
            this->RunQuery(query);
        }
        if (!this->HasTable(prefix + "geoip_city_region")) {
            query = "CREATE TABLE `" + prefix + "geoip_city_region` ("
                    "`country` CHAR(2) NOT NULL,"
                    "`region` CHAR(2) NOT NULL,"
                    "`regionname` VARCHAR(100) NOT NULL,"
                    "PRIMARY KEY (`country`,`region`)"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
            this->RunQuery(query);
        }
    }
    if (!this->HasTable(prefix + "server")) {
        query = "CREATE TABLE `" + prefix + "server` ("
                "`id` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` varchar(64) NOT NULL,"
                "`hops` tinyint(3) NOT NULL,"
                "`comment` varchar(255) NOT NULL,"
                "`link_time` datetime DEFAULT NULL,"
                "`split_time` datetime DEFAULT NULL,"
                "`version` varchar(127) DEFAULT NULL,"
                "`currentusers` int(15) DEFAULT 0,"
                "`online` enum('Y','N') NOT NULL DEFAULT 'Y',"
                "`ulined` enum('Y','N') NOT NULL DEFAULT 'N',"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        this->RunQuery(query);
    }
    if (!this->HasTable(prefix + "chan")) {
        query = "CREATE TABLE `" + prefix + "chan` ("
                "`chanid` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`channel` varchar(255) NOT NULL,"
                "`topic` varchar(512) DEFAULT NULL,"
                "`topicauthor` varchar(255) DEFAULT NULL,"
                "`topictime` datetime DEFAULT NULL,"
                "`modes` varchar(512) DEFAULT NULL,"
                "PRIMARY KEY (`chanid`),"
                "UNIQUE KEY `channel`(`channel`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        this->RunQuery(query);
    }
    if (!this->HasTable(prefix + "user")) {
        query = "CREATE TABLE `" + prefix + "user` ("
                "`nickid` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`nick` varchar(255) NOT NULL DEFAULT '',"
                "`host` varchar(255) NOT NULL DEFAULT '',"
                "`vhost` varchar(255) NOT NULL DEFAULT '',"
                "`chost` varchar(255) NOT NULL DEFAULT '',"
                "`realname` varchar(255) NOT NULL DEFAULT '',"
                "`ip` varchar(255) NOT NULL DEFAULT '',"
                "`ident` varchar(32) NOT NULL DEFAULT '',"
                "`vident` varchar(32) NOT NULL DEFAULT '',"
                "`modes` varchar(255) NOT NULL DEFAULT '',"
                "`account` varchar(255) NOT NULL DEFAULT '',"
                "`secure` enum('Y','N') NOT NULL DEFAULT 'N',"
                "`fingerprint` varchar(128) NOT NULL DEFAULT '',"
                "`signon` datetime DEFAULT NULL,"
                "`server` varchar(255) NOT NULL DEFAULT '',"
                "`servid` int(11) UNSIGNED NOT NULL DEFAULT '0',"
                "`uuid` varchar(32) NOT NULL DEFAULT '',"
                "`oper` enum('Y','N') NOT NULL DEFAULT 'N',"
                "`away` enum('Y','N') NOT NULL DEFAULT 'N',"
                "`awaymsg` varchar(255) NOT NULL DEFAULT '',"
                "`version` varchar(255) NOT NULL DEFAULT '',"
                "`geocode` varchar(16) NOT NULL DEFAULT '',"
                "`geocountry` varchar(64) NOT NULL DEFAULT '',"
                "`georegion` varchar(100) NOT NULL DEFAULT '',"
                "`geocity` varchar(128) NOT NULL DEFAULT '',"
                "`locId` INT UNSIGNED,"
                "PRIMARY KEY (`nickid`),"
                "UNIQUE KEY `nick` (`nick`),"
                "KEY `servid` (`servid`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        this->RunQuery(query);
    }
    if (!this->HasTable(prefix + "ison")) {
        query = "CREATE TABLE `" + prefix + "ison` ("
                "`nickid` int(11) unsigned NOT NULL default '0',"
                "`chanid` int(11) unsigned NOT NULL default '0',"
                "`modes` varchar(255) NOT NULL default '',"
                "PRIMARY KEY  (`nickid`,`chanid`),"
                "KEY `modes` (`modes`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        this->RunQuery(query);
    }
    if (!this->HasTable(prefix + "maxusers")) {
        query = "CREATE TABLE `" + prefix + "maxusers` ("
                "`name` VARCHAR(255) NOT NULL,"
                "`maxusers` INT(15) NOT NULL,"
                "`maxtime` DATETIME NOT NULL,"
                "`lastused` DATETIME NOT NULL,"
                "UNIQUE KEY `name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        this->RunQuery(query);
    }
    if (this->HasProcedure(prefix + "UserConnect")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "UserConnect"));
    }

    if (GeoIPDB.equals_ci("country"))
        geoquery = "UPDATE `" + prefix + "user` AS u "
                   "JOIN ( SELECT `countrycode`, `countryname` "
                   "FROM `" + prefix + "geoip_country` "
                   "WHERE INET_ATON(ip_) <= `end` "
                   "AND `start` <= INET_ATON(ip_) "
                   "ORDER BY `end` ASC LIMIT 1 ) as c "
                   "SET u.geocode = c.countrycode, u.geocountry = c.countryname "
                   "WHERE u.nick = nick_; ";
    else if (GeoIPDB.equals_ci("city"))
        geoquery = "UPDATE `" + prefix + "user` as u "
                   "JOIN ( SELECT * FROM `" + prefix + "geoip_city_location` "
                   "WHERE `locID` = ( SELECT `locID` "
                   "FROM `" + prefix + "geoip_city_blocks` "
                   "WHERE INET_ATON(ip_) <= `end` "
                   "AND `start` <= INET_ATON(ip_) "
                   "ORDER BY `end` ASC LIMIT 1 ) "
                   ") as l "
                   "SET u.geocode = l.country, "
                   "u.geocity = l.city, "
                   "u.locID = l.locID, "
                   "u.georegion = ( SELECT `regionname` "
                   "FROM `" + prefix + "geoip_city_region` "
                   "WHERE `country` = l.country "
                   "AND `region` = l.region )"
                   "WHERE u.nick = nick_;";

    query = "CREATE PROCEDURE `" + prefix + "UserConnect`"
            "(nick_ varchar(255), host_ varchar(255), vhost_ varchar(255), "
            "chost_ varchar(255), realname_ varchar(255), ip_ varchar(255), "
            "ident_ varchar(255), vident_ varchar(255), account_ varchar(255), "
            "secure_ enum('Y','N'), fingerprint_ varchar(255), signon_ int(15), "
            "server_ varchar(255), uuid_ varchar(32), modes_ varchar(255), "
            "oper_ enum('Y','N')) "
            "BEGIN "
            "DECLARE cur int(15);"
            "DECLARE max int(15);"
            "INSERT INTO `" + prefix + "user` "
            "(nick, host, vhost, chost, realname, ip, ident, vident, account, "
            "secure, fingerprint, signon, server, uuid, modes, oper) "
            "VALUES (nick_, host_, vhost_, chost_, realname_, ip_, ident_, vident_, "
            "account_, secure_, fingerprint_, FROM_UNIXTIME(signon_), server_, "
            "uuid_, modes_, oper_) "
            "ON DUPLICATE KEY UPDATE host=VALUES(host), vhost=VALUES(vhost), "
            "chost=VALUES(chost), realname=VALUES(realname), ip=VALUES(ip), "
            "ident=VALUES(ident), vident=VALUES(vident), account=VALUES(account), "
            "secure=VALUES(secure), fingerprint=VALUES(fingerprint), signon=VALUES(signon), "
            "server=VALUES(server), uuid=VALUES(uuid), modes=VALUES(modes), "
            "oper=VALUES(oper);"
            "UPDATE `" + prefix + "user` as `u`, `" + prefix + "server` as `s`"
            "SET u.servid = s.id, "
            "s.currentusers = s.currentusers + 1 "
            "WHERE s.name = server_ AND u.nick = nick_;"
            "SELECT `currentusers` INTO cur FROM `" + prefix + "server` WHERE name=server_;"
            "SELECT `maxusers` INTO max FROM `" + prefix + "maxusers` WHERE name=server_;"
            "IF found_rows() AND cur <= max THEN "
            "UPDATE `" + prefix + "maxusers` SET lastused=now() WHERE name=server_;"
            "ELSE "
            "INSERT INTO `" + prefix + "maxusers` (name, maxusers, maxtime, lastused) "
            "VALUES ( server_, cur, now(), now() ) "
            "ON DUPLICATE KEY UPDATE "
            "name=VALUES(name), maxusers=VALUES(maxusers),"
            "maxtime=VALUES(maxtime), lastused=VALUES(lastused);"
            "END IF;"
            + geoquery +
            "END";
    this->RunQuery(query);

    if (this->HasProcedure(prefix + "ServerQuit")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "ServerQuit"));
    }
    query = "CREATE PROCEDURE " + prefix + "ServerQuit(sname_ varchar(255)) "
            "BEGIN "
            /* 1.
             * remove all users on the splitting server from the ison table
             */
            "DELETE i FROM `" + prefix + "ison` AS i "
            "INNER JOIN `" + prefix + "server` AS s "
            "INNER JOIN `" + prefix + "user` AS u "
            "WHERE i.nickid = u.nickid "
            "AND u.servid = s.id "
            "AND s.name = sname_;"

            /* 2.
             * remove all users on the splitting server from the user table
             */
            "DELETE u FROM `" + prefix + "user` AS u "
            "INNER JOIN `" + prefix + "server` AS s "
            "WHERE s.id = u.servid "
            "AND s.name = sname_;"

            /* 3.
             * on the splitting server, set usercount = 0, split_time = now(), online = 'N'
             */
            "UPDATE `" + prefix +
            "server` SET currentusers = 0, split_time = now(), online = 'N' "
            "WHERE name = sname_;"
            "END;"; // end of the procedure
    this->RunQuery(query);


    if (this->HasProcedure(prefix + "UserQuit")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "UserQuit"));
    }
    query = "CREATE PROCEDURE `" + prefix + "UserQuit`"
            "(nick_ varchar(255)) "
            "BEGIN "
            /* decrease usercount on the server where the user was on */
            "UPDATE `" + prefix + "user` AS `u`, `" + prefix + "server` AS `s` "
            "SET s.currentusers = s.currentusers - 1 "
            "WHERE u.nick=nick_ AND u.servid = s.id; "
            /* remove from all channels where the user was on */
            "DELETE i FROM `" + prefix + "ison` AS i "
            "INNER JOIN `" + prefix + "user` as u "
            "WHERE u.nick = nick_ "
            "AND i.nickid = u.nickid;"
            /* remove the user from the user table */
            "DELETE FROM `" + prefix + "user` WHERE nick = nick_; "
            "END";
    this->RunQuery(query);

    if (this->HasProcedure(prefix + "ShutDown")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "ShutDown"));
    }
    query = "CREATE PROCEDURE `" + prefix + "ShutDown`()"
            "BEGIN "
            "UPDATE `" +  prefix + "server` "
            "SET currentusers=0, online='N', split_time=now();"
            "TRUNCATE TABLE `" + prefix + "user`;"
            "TRUNCATE TABLE `" + prefix + "chan`;"
            "TRUNCATE TABLE `" + prefix + "ison`;"
            "END";
    this->RunQuery(query);

    if (this->HasProcedure(prefix + "JoinUser")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "JoinUser"));
    }
    query = "CREATE PROCEDURE `"+ prefix + "JoinUser`"
            "(nick_ varchar(255), channel_ varchar(255), modes_ varchar(255)) "
            "BEGIN "
            "DECLARE cur int(15);"
            "DECLARE max int(15);"
            "INSERT INTO `" + prefix + "ison` (nickid, chanid, modes) "
            "SELECT u.nickid, c.chanid, modes_ "
            "FROM " + prefix + "user AS u, " + prefix + "chan AS c "
            "WHERE u.nick=nick_ AND c.channel=channel_;"
            "SELECT count(i.chanid) INTO cur "
            "FROM `" + prefix + "chan` AS c, " +  prefix + "ison AS i "
            "WHERE i.chanid = c.chanid AND c.channel=channel_;"
            "SELECT `maxusers` INTO max FROM `" + prefix + "maxusers` WHERE name=channel_;"
            "IF found_rows() AND cur <= max THEN "
            "UPDATE `" + prefix + "maxusers` SET lastused=now() WHERE name=channel_;"
            "ELSE "
            "INSERT INTO `" + prefix + "maxusers` (name, maxusers, maxtime, lastused) "
            "VALUES ( channel_, cur, now(), now() ) "
            "ON DUPLICATE KEY UPDATE "
            "name=VALUES(name), maxusers=VALUES(maxusers),"
            "maxtime=VALUES(maxtime), lastused=VALUES(lastused);"
            "END IF;"
            "END";
    this->RunQuery(query);

    if (this->HasProcedure(prefix + "PartUser")) {
        this->RunQuery(SQL::Query("DROP PROCEDURE " + prefix + "PartUser"));
    }
    query = "CREATE PROCEDURE `" + prefix + "PartUser`"
            "(nick_ varchar(255), channel_ varchar(255)) "
            "BEGIN "
            "DELETE i FROM `" + prefix + "ison` AS i "
            "INNER JOIN `" + prefix + "user` AS u "
            "INNER JOIN `" + prefix + "chan` AS c "
            "WHERE i.nickid = u.nickid "
            "AND u.nick = nick_ "
            "AND i.chanid = c.chanid "
            "AND c.channel = channel_;"
            "END";
    this->RunQuery(query);
}
