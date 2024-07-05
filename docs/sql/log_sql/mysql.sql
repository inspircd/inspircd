CREATE TABLE IF NOT EXISTS `ircd_log` (
  `time` datetime NOT NULL,
  `type` varchar(50) NOT NULL,
  `message` text NOT NULL
);
