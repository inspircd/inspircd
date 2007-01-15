-- MySQL dump 9.11
--
-- Host: localhost    Database: brain
-- ------------------------------------------------------
-- Server version	4.0.20

--
-- Table structure for table `ircd_opers`
--

CREATE TABLE ircd_opers (
  id bigint(20) NOT NULL auto_increment,
  username text,
  password text,
  hostname text,
  type text,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Dumping data for table `ircd_opers`
--


