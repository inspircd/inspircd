-- MySQL dump 9.11
--
-- Host: localhost    Database: brain
-- ------------------------------------------------------
-- Server version	4.0.20

--
-- Table structure for table `ircd_log`
--

CREATE TABLE ircd_log (
  id bigint(20) NOT NULL auto_increment,
  category_id bigint(20) NOT NULL default '0',
  nick bigint(20) default NULL,
  host bigint(20) default NULL,
  source bigint(20) default NULL,
  date bigint(20) NOT NULL default '0',
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Dumping data for table `ircd_log`
--


--
-- Table structure for table `ircd_log_categories`
--

CREATE TABLE ircd_log_categories (
  category_id bigint(20) NOT NULL default '0',
  category text NOT NULL,
  PRIMARY KEY  (category_id)
) TYPE=MyISAM;

--
-- Dumping data for table `ircd_log_categories`
--

INSERT INTO ircd_log_categories VALUES (1,'Oper');
INSERT INTO ircd_log_categories VALUES (2,'Kill');
INSERT INTO ircd_log_categories VALUES (3,'Server Link');
INSERT INTO ircd_log_categories VALUES (4,'G/Z/K/E Line');
INSERT INTO ircd_log_categories VALUES (5,'Connect');
INSERT INTO ircd_log_categories VALUES (6,'Disconnect');
INSERT INTO ircd_log_categories VALUES (7,'Flooding');
INSERT INTO ircd_log_categories VALUES (8,'Load Module');

--
-- Table structure for table `ircd_log_actors`
--

CREATE TABLE ircd_log_actors (
  id bigint(20) NOT NULL auto_increment,
  actor text,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Dumping data for table `ircd_log_actors`
--


--
-- Table structure for table `ircd_log_hosts`
--

CREATE TABLE ircd_log_hosts (
  id bigint(20) NOT NULL auto_increment,
  hostname text,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Dumping data for table `ircd_log_hosts`
--


