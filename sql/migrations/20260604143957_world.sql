DROP PROCEDURE IF EXISTS add_migration;
DELIMITER ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20260604143957');
IF v = 0 THEN
INSERT INTO `migrations` VALUES ('20260604143957');
-- Add your query below.

UPDATE `worldbot_travelnode` SET `name` = CONCAT(`name`, ' horde')
WHERE id = 266 OR id = 311 OR id = 617 OR id = 689 OR id = 842 OR id = 969 OR id = 1570;

UPDATE `worldbot_travelnode` SET `name` = CONCAT(`name`, ' alliance')
WHERE id = 267 OR id = 310 OR id = 616 OR id = 690 OR id = 841 OR id = 968 OR id = 1569;

-- End of migration.
END IF;
END??
DELIMITER ;
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;