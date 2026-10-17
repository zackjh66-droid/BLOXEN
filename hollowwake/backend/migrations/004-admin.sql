CREATE TABLE IF NOT EXISTS `admin_audit` (
	`id` text PRIMARY KEY NOT NULL,
	`actor` text NOT NULL,
	`action` text NOT NULL,
	`target` text NOT NULL,
	`detail` text NOT NULL,
	`created_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `admin_document` (
	`kind` text NOT NULL,
	`id` text PRIMARY KEY NOT NULL,
	`data` text NOT NULL,
	`revision` integer NOT NULL,
	`updated_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `admin_nonce` (
	`nonce` text PRIMARY KEY NOT NULL,
	`expires_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `project_inventory` (
	`id` text PRIMARY KEY NOT NULL,
	`user_id` text NOT NULL,
	`cosmetic_id` text NOT NULL,
	`granted_at` integer NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX IF NOT EXISTS `inventory_user_cosmetic` ON `project_inventory` (`user_id`,`cosmetic_id`);--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `project_moderation` (
	`user_id` text PRIMARY KEY NOT NULL,
	`banned_until` integer NOT NULL,
	`reason` text NOT NULL,
	`updated_at` integer NOT NULL
);
