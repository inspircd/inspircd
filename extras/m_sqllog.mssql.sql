CREATE TABLE [dbo].[ircd_log] (
  [id] int IDENTITY(1, 1) NOT NULL,
  [category_id] int DEFAULT 0 NOT NULL,
  [nick] int NULL,
  [host] int NULL,
  [source] int NULL,
  [dtime] int DEFAULT 0 NOT NULL,
  PRIMARY KEY CLUSTERED ([id])
)


CREATE TABLE [dbo].[ircd_log_categories] (
  [category_id] int DEFAULT 0 NOT NULL,
  [category] varchar(255) NOT NULL,
  PRIMARY KEY CLUSTERED ([category_id])
)


/* Data for the `dbo.ircd_log_categories` table  (Records 1 - 8) */
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (1, N'Oper')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (2, N'Kill')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (3, N'Server Link')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (4, N'G/Z/K/E Line')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (5, N'Connect')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (6, N'Disconnect')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (7, N'Flooding')
INSERT INTO [dbo].[ircd_log_categories] ([category_id], [category]) VALUES (8, N'Load Module')


CREATE TABLE [dbo].[ircd_log_actors] (
  [id] int IDENTITY(1, 1) NOT NULL,
  [actor] varchar(255) NULL,
  PRIMARY KEY CLUSTERED ([id])
)


CREATE TABLE [dbo].[ircd_log_hosts] (
  [id] int IDENTITY(1, 1) NOT NULL,
  [hostname] varchar(255) NULL,
  PRIMARY KEY CLUSTERED ([id])
)

