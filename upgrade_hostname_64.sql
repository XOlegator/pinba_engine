ALTER TABLE `request`
    MODIFY `hostname` varchar(64) DEFAULT NULL;

ALTER TABLE `report_by_hostname`
    MODIFY `hostname` varchar(64) DEFAULT NULL;

ALTER TABLE `report_by_hostname_and_script`
    MODIFY `hostname` varchar(64) DEFAULT NULL;

ALTER TABLE `report_by_hostname_and_server`
    MODIFY `hostname` varchar(64) DEFAULT NULL;

ALTER TABLE `report_by_hostname_server_and_script`
    MODIFY `hostname` varchar(64) DEFAULT NULL;
