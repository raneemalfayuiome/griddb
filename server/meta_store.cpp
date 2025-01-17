﻿/*
	Copyright (c) 2018 TOSHIBA Digital Solutions Corporation

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
	@file
	@brief Implementation of metadata store
*/

#include "meta_store.h"

#include "time_series.h"
#include "lexer.h"

#include "picojson.h"
#include "sql_processor.h"
#include "sql_execution.h"
#include "sql_service.h"
#include "sql_job_manager.h"
#include "nosql_command.h"
#include "key_data_store.h"
UTIL_TRACER_DECLARE(SQL_SERVICE);


MetaColumnInfo::MetaColumnInfo() :
		id_(UNDEF_COLUMNID),
		refId_(UNDEF_COLUMNID),
		type_(COLUMN_TYPE_NULL),
		nullable_(false) {
}


MetaColumnInfo::NameInfo::NameInfo() :
		forContainer_(NULL),
		forTable_(NULL) {
}


MetaContainerInfo::MetaContainerInfo() :
		id_(UNDEF_CONTAINERID),
		refId_(UNDEF_CONTAINERID),
		versionId_(UNDEF_SCHEMAVERSIONID),
		forCore_(false),
		internal_(false),
		adminOnly_(false),
		nodeDistribution_(false),
		columnList_(NULL),
		columnCount_(0) {
}

bool MetaContainerInfo::isEmpty() const {
	return (columnCount_ == 0);
}


MetaContainerInfo::NameInfo::NameInfo() :
		neutral_(NULL),
		forContainer_(NULL),
		forTable_(NULL) {
}


MetaContainerInfo::CommonColumnInfo::CommonColumnInfo() :
		dbNameColumn_(UNDEF_COLUMNID),
		containerNameColumn_(UNDEF_COLUMNID),
		partitionIndexColumn_(UNDEF_COLUMNID),
		containerIdColumn_(UNDEF_COLUMNID) {
}


const util::NameCoderEntry<MetaType::ContainerMeta>
		MetaType::Coders::LIST_CONTAINER[] = {
	UTIL_NAME_CODER_ENTRY(CONTAINER_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(CONTAINER_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_ATTRIBUTE),
	UTIL_NAME_CODER_ENTRY(CONTAINER_TYPE_NAME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_DATA_AFFINITY),
	UTIL_NAME_CODER_ENTRY(CONTAINER_EXPIRATION_TIME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_EXPIRATION_UNIT),
	UTIL_NAME_CODER_ENTRY(CONTAINER_EXPIRATION_DIVISION),
	UTIL_NAME_CODER_ENTRY(CONTAINER_COMPRESSION_METHOD),
	UTIL_NAME_CODER_ENTRY(CONTAINER_COMPRESSION_SIZE),
	UTIL_NAME_CODER_ENTRY(CONTAINER_COMPRESSION_UNIT),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_TYPE1),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_COLUMN1),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_INTERVAL1),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_UNIT1),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_DIVISION1),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_TYPE2),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_COLUMN2),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_INTERVAL2),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_UNIT2),
	UTIL_NAME_CODER_ENTRY(CONTAINER_PARTITION_DIVISION2),
	UTIL_NAME_CODER_ENTRY(CONTAINER_CLUSTER_PARTITION),
	UTIL_NAME_CODER_ENTRY(CONTAINER_EXPIRATION_TYPE)
};
const util::NameCoderEntry<MetaType::ColumnMeta>
		MetaType::Coders::LIST_COLUMN[] = {
	UTIL_NAME_CODER_ENTRY(COLUMN_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(COLUMN_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(COLUMN_CONTAINER_ATTRIBUTE),
	UTIL_NAME_CODER_ENTRY(COLUMN_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(COLUMN_ORDINAL),
	UTIL_NAME_CODER_ENTRY(COLUMN_SQL_TYPE),
	UTIL_NAME_CODER_ENTRY(COLUMN_TYPE_NAME),
	UTIL_NAME_CODER_ENTRY(COLUMN_NAME),
	UTIL_NAME_CODER_ENTRY(COLUMN_KEY),
	UTIL_NAME_CODER_ENTRY(COLUMN_NULLABLE),
	UTIL_NAME_CODER_ENTRY(COLUMN_KEY_SEQUENCE),
	UTIL_NAME_CODER_ENTRY(COLUMN_COMPRESSION_RELATIVE),
	UTIL_NAME_CODER_ENTRY(COLUMN_COMPRESSION_RATE),
	UTIL_NAME_CODER_ENTRY(COLUMN_COMPRESSION_SPAN),
	UTIL_NAME_CODER_ENTRY(COLUMN_COMPRESSION_WIDTH)
};
const util::NameCoderEntry<MetaType::IndexMeta>
		MetaType::Coders::LIST_INDEX[] = {
	UTIL_NAME_CODER_ENTRY(INDEX_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(INDEX_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(INDEX_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(INDEX_NAME),
	UTIL_NAME_CODER_ENTRY(INDEX_ORDINAL),
	UTIL_NAME_CODER_ENTRY(INDEX_COLUMN_NAME),
	UTIL_NAME_CODER_ENTRY(INDEX_TYPE),
	UTIL_NAME_CODER_ENTRY(INDEX_TYPE_NAME)
};
const util::NameCoderEntry<MetaType::TriggerMeta>
		MetaType::Coders::LIST_TRIGGER[] = {
	UTIL_NAME_CODER_ENTRY(TRIGGER_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(TRIGGER_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(TRIGGER_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(TRIGGER_ORDINAL),
	UTIL_NAME_CODER_ENTRY(TRIGGER_NAME),
	UTIL_NAME_CODER_ENTRY(TRIGGER_EVENT_TYPE),
	UTIL_NAME_CODER_ENTRY(TRIGGER_COLUMN_NAME),
	UTIL_NAME_CODER_ENTRY(TRIGGER_TYPE),
	UTIL_NAME_CODER_ENTRY(TRIGGER_URI),
	UTIL_NAME_CODER_ENTRY(TRIGGER_JMS_DESTINATION_TYPE),
	UTIL_NAME_CODER_ENTRY(TRIGGER_JMS_DESTINATION_NAME),
	UTIL_NAME_CODER_ENTRY(TRIGGER_USER),
	UTIL_NAME_CODER_ENTRY(TRIGGER_PASSWORD)
};

const util::NameCoderEntry<MetaType::ErasableMeta>
		MetaType::Coders::LIST_ERASABLE[] = {
	UTIL_NAME_CODER_ENTRY(ERASABLE_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(ERASABLE_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_TYPE_NAME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_CONTAINER_ID),
	UTIL_NAME_CODER_ENTRY(ERASABLE_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_PARTITION_NAME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_CLUSTER_PARTITION),
	UTIL_NAME_CODER_ENTRY(ERASABLE_LARGE_CONTAINER_ID),
	UTIL_NAME_CODER_ENTRY(ERASABLE_SCHEMA_VERSION_ID),
	UTIL_NAME_CODER_ENTRY(ERASABLE_INIT_SCHEMA_STATUS),
	UTIL_NAME_CODER_ENTRY(ERASABLE_EXPIRATION_TYPE),
	UTIL_NAME_CODER_ENTRY(ERASABLE_LOWER_BOUNDARY_TIME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_UPPER_BOUNDARY_TIME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_EXPIRATION_TIME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_ERASABLE_TIME),
	UTIL_NAME_CODER_ENTRY(ERASABLE_ROW_INDEX_OID),
	UTIL_NAME_CODER_ENTRY(ERASABLE_MVCC_INDEX_OID)
};

const util::NameCoderEntry<MetaType::EventMeta>
		MetaType::Coders::LIST_EVENT[] = {
	UTIL_NAME_CODER_ENTRY(EVENT_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(EVENT_NODE_PORT),
	UTIL_NAME_CODER_ENTRY(EVENT_START_TIME),
	UTIL_NAME_CODER_ENTRY(EVENT_APPLICATION_NAME),
	UTIL_NAME_CODER_ENTRY(EVENT_SERVICE_TYPE),
	UTIL_NAME_CODER_ENTRY(EVENT_EVENT_TYPE),
	UTIL_NAME_CODER_ENTRY(EVENT_WORKER_INDEX),
	UTIL_NAME_CODER_ENTRY(EVENT_CLUSTER_PARTITION_INDEX)
};

const util::NameCoderEntry<MetaType::SocketMeta>
		MetaType::Coders::LIST_SOCKET[] = {
	UTIL_NAME_CODER_ENTRY(SOCKET_SERVICE_TYPE),
	UTIL_NAME_CODER_ENTRY(SOCKET_TYPE),
	UTIL_NAME_CODER_ENTRY(SOCKET_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(SOCKET_NODE_PORT),
	UTIL_NAME_CODER_ENTRY(SOCKET_REMOTE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(SOCKET_REMOTE_PORT),
	UTIL_NAME_CODER_ENTRY(SOCKET_APPLICATION_NAME),
	UTIL_NAME_CODER_ENTRY(SOCKET_CREATION_TIME),
	UTIL_NAME_CODER_ENTRY(SOCKET_DISPATCHING_EVENT_COUNT),
	UTIL_NAME_CODER_ENTRY(SOCKET_SENDING_EVENT_COUNT)
};

const util::NameCoderEntry<MetaType::ContainerStatsMeta>
		MetaType::Coders::LIST_CONTAINER_STATS[] = {
	UTIL_NAME_CODER_ENTRY(CONTAINER_STATS_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(CONTAINER_STATS_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_STATS_NAME),
	UTIL_NAME_CODER_ENTRY(CONTAINER_STATS_GROUPID),
	UTIL_NAME_CODER_ENTRY(CONTAINER_STATS_NUM_ROWS)
};

const util::NameCoderEntry<MetaType::ClusterPartitionMeta>
		MetaType::Coders::LIST_CLUSTER_PARTITION[] = {
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_CLUSTER_PARTITION_INDEX),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_ROLE),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_NODE_PORT),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_LSN),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_STATUS),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_BLOCK_CATEGORY),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_STORE_USE),
	UTIL_NAME_CODER_ENTRY(CLUSTER_PARTITION_STORE_OBJECT_USE),
};

const util::NameCoderEntry<MetaType::PartitionMeta>
		MetaType::Coders::LIST_PARTITION[] = {
	UTIL_NAME_CODER_ENTRY(PARTITION_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(PARTITION_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_ORDINAL),
	UTIL_NAME_CODER_ENTRY(PARTITION_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_BOUNDARY_VALUE1),
	UTIL_NAME_CODER_ENTRY(PARTITION_BOUNDARY_VALUE2),
	UTIL_NAME_CODER_ENTRY(PARTITION_NODE_AFFINITY),
	UTIL_NAME_CODER_ENTRY(PARTITION_CLUSTER_PARTITION_INDEX),
	UTIL_NAME_CODER_ENTRY(PARTITION_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(PARTITION_STATUS)
};

const util::NameCoderEntry<MetaType::ViewMeta>
		MetaType::Coders::LIST_VIEW[] = {
	UTIL_NAME_CODER_ENTRY(VIEW_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(VIEW_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(VIEW_NAME),
	UTIL_NAME_CODER_ENTRY(VIEW_DEFINITION)
};

const util::NameCoderEntry<MetaType::SQLMeta>
		MetaType::Coders::LIST_SQL[] = {
	UTIL_NAME_CODER_ENTRY(SQL_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(SQL_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(SQL_NODE_PORT),
	UTIL_NAME_CODER_ENTRY(SQL_START_TIME),
	UTIL_NAME_CODER_ENTRY(SQL_APPLICATION_NAME),
	UTIL_NAME_CODER_ENTRY(SQL_SQL),
	UTIL_NAME_CODER_ENTRY(SQL_QUERY_ID),
	UTIL_NAME_CODER_ENTRY(SQL_JOB_ID),
};

const util::NameCoderEntry<MetaType::PartitionStatsMeta>
		MetaType::Coders::LIST_PARTITION_STATS[] = {
	UTIL_NAME_CODER_ENTRY(PARTITION_STATS_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(PARTITION_STATS_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_STATS_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_STATS_NAME),
	UTIL_NAME_CODER_ENTRY(PARTITION_STATS_NUM_ROWS),
};


const util::NameCoder<MetaType::ContainerMeta, MetaType::END_CONTAINER>
		MetaType::Coders::CODER_CONTAINER(LIST_CONTAINER, 1);
const util::NameCoder<MetaType::ColumnMeta, MetaType::END_COLUMN>
		MetaType::Coders::CODER_COLUMN(LIST_COLUMN, 1);
const util::NameCoder<MetaType::IndexMeta, MetaType::END_INDEX>
		MetaType::Coders::CODER_INDEX(LIST_INDEX, 1);
const util::NameCoder<MetaType::TriggerMeta, MetaType::END_TRIGGER>
		 MetaType::Coders::CODER_TRIGGER(LIST_TRIGGER, 1);
const util::NameCoder<MetaType::ErasableMeta, MetaType::END_ERASABLE>
		 MetaType::Coders::CODER_ERASABLE(LIST_ERASABLE, 1);
const util::NameCoder<MetaType::EventMeta, MetaType::END_EVENT>
		MetaType::Coders::CODER_EVENT(LIST_EVENT, 1);
const util::NameCoder<MetaType::SocketMeta, MetaType::END_SOCKET>
		MetaType::Coders::CODER_SOCKET(LIST_SOCKET, 1);
const util::NameCoder<MetaType::ContainerStatsMeta, MetaType::END_CONTAINER_STATS>
		MetaType::Coders::CODER_CONTAINER_STATS(LIST_CONTAINER_STATS, 1);
const util::NameCoder<MetaType::ClusterPartitionMeta, MetaType::END_CLUSTER_PARTITION>
		MetaType::Coders::CODER_CLUSTER_PARTITION(LIST_CLUSTER_PARTITION, 1);

const util::NameCoder<MetaType::PartitionMeta, MetaType::END_PARTITION>
		MetaType::Coders::CODER_PARTITION(LIST_PARTITION, 1);
const util::NameCoder<MetaType::ViewMeta, MetaType::END_VIEW>
		MetaType::Coders::CODER_VIEW(LIST_VIEW, 1);
const util::NameCoder<MetaType::SQLMeta, MetaType::END_SQL>
		MetaType::Coders::CODER_SQL(LIST_SQL, 1);
const util::NameCoder<MetaType::PartitionStatsMeta, MetaType::END_PARTITION_STATS>
		MetaType::Coders::CODER_PARTITION_STATS(LIST_PARTITION_STATS, 1);

const util::NameCoderEntry<MetaType::StringConstants>
		MetaType::Coders::LIST_STR[] = {
	UTIL_NAME_CODER_ENTRY(STR_CONTAINER_NAME),
	UTIL_NAME_CODER_ENTRY(STR_CONTAINER_OPTIONAL_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_TABLE_NAME),
	UTIL_NAME_CODER_ENTRY(STR_TABLE_OPTIONAL_TYPE),

	UTIL_NAME_CODER_ENTRY(STR_KEY_SEQ),
	UTIL_NAME_CODER_ENTRY(STR_COLUMN_NAME),
	UTIL_NAME_CODER_ENTRY(STR_NULLABLE),
	UTIL_NAME_CODER_ENTRY(STR_ORDINAL_POSITION),
	UTIL_NAME_CODER_ENTRY(STR_TYPE_NAME),
	UTIL_NAME_CODER_ENTRY(STR_INDEX_NAME),

	UTIL_NAME_CODER_ENTRY(STR_DATABASE_NAME),
	UTIL_NAME_CODER_ENTRY(STR_DATA_AFFINITY),
	UTIL_NAME_CODER_ENTRY(STR_DATA_GROUPID),
	UTIL_NAME_CODER_ENTRY(STR_EXPIRATION_TIME),
	UTIL_NAME_CODER_ENTRY(STR_EXPIRATION_TIME_UNIT),
	UTIL_NAME_CODER_ENTRY(STR_EXPIRATION_DIVISION_COUNT),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_METHOD),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_WINDOW_SIZE),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_WINDOW_SIZE_UNIT),
	UTIL_NAME_CODER_ENTRY(STR_CLUSTER_PARTITION_INDEX),
	UTIL_NAME_CODER_ENTRY(STR_EXPIRATION_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_SEQ),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_NAME),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_BOUNDARY_VALUE),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_BOUNDARY_VALUE),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_NODE_AFFINITY),
	UTIL_NAME_CODER_ENTRY(STR_DATABASE_MAJOR_VERSION),
	UTIL_NAME_CODER_ENTRY(STR_DATABASE_MINOR_VERSION),
	UTIL_NAME_CODER_ENTRY(STR_EXTRA_NAME_CHARACTERS),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_RELATIVE),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_RATE),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_SPAN),
	UTIL_NAME_CODER_ENTRY(STR_COMPRESSION_WIDTH),
	UTIL_NAME_CODER_ENTRY(STR_INDEX_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_TRIGGER_NAME),
	UTIL_NAME_CODER_ENTRY(STR_EVENT_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_TRIGGER_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_URI),
	UTIL_NAME_CODER_ENTRY(STR_JMS_DESTINATION_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_JMS_DESTINATION_NAME),
	UTIL_NAME_CODER_ENTRY(STR_USER),
	UTIL_NAME_CODER_ENTRY(STR_PASSWORD)
	,
	UTIL_NAME_CODER_ENTRY(STR_ATTRIBUTE),
	UTIL_NAME_CODER_ENTRY(STR_DATABASE_ID),
	UTIL_NAME_CODER_ENTRY(STR_CONTAINER_ID),
	UTIL_NAME_CODER_ENTRY(STR_LARGE_CONTAINER_ID),
	UTIL_NAME_CODER_ENTRY(STR_SCHEMA_VERSION_ID),
	UTIL_NAME_CODER_ENTRY(STR_INIT_SCHEMA_STATUS),
	UTIL_NAME_CODER_ENTRY(STR_LOWER_BOUNDARY_TIME),
	UTIL_NAME_CODER_ENTRY(STR_UPPER_BOUNDARY_TIME),
	UTIL_NAME_CODER_ENTRY(STR_ERASABLE_TIME),
	UTIL_NAME_CODER_ENTRY(STR_ROW_INDEX_OID),
	UTIL_NAME_CODER_ENTRY(STR_MVCC_INDEX_OID)
	,
	UTIL_NAME_CODER_ENTRY(STR_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(STR_NODE_PORT),
	UTIL_NAME_CODER_ENTRY(STR_START_TIME),
	UTIL_NAME_CODER_ENTRY(STR_APPLICATION_NAME),
	UTIL_NAME_CODER_ENTRY(STR_SERVICE_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_WORKER_INDEX),
	UTIL_NAME_CODER_ENTRY(STR_SOCKET_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_REMOTE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(STR_REMOTE_PORT),
	UTIL_NAME_CODER_ENTRY(STR_CREATION_TIME),
	UTIL_NAME_CODER_ENTRY(STR_DISPATCHING_EVENT_COUNT),
	UTIL_NAME_CODER_ENTRY(STR_SENDING_EVENT_COUNT)

	,
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_COLUMN),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_INTERVAL_VALUE),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_INTERVAL_UNIT),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_DIVISION_COUNT),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_TYPE),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_COLUMN),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_INTERVAL_VALUE),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_INTERVAL_UNIT),
	UTIL_NAME_CODER_ENTRY(STR_SUBPARTITION_DIVISION_COUNT),
	UTIL_NAME_CODER_ENTRY(STR_CLUSTER_NODE_ADDRESS),
	UTIL_NAME_CODER_ENTRY(STR_PARTITION_STATUS)
	,
	UTIL_NAME_CODER_ENTRY(STR_VIEW_NAME),  
	UTIL_NAME_CODER_ENTRY(STR_VIEW_DEFINITION),

	UTIL_NAME_CODER_ENTRY(STR_SQL),  
	UTIL_NAME_CODER_ENTRY(STR_QUERY_ID),
	UTIL_NAME_CODER_ENTRY(STR_JOB_ID),

	UTIL_NAME_CODER_ENTRY(STR_NUM_ROWS),
	UTIL_NAME_CODER_ENTRY(STR_ROLE),
	UTIL_NAME_CODER_ENTRY(STR_LSN),
	UTIL_NAME_CODER_ENTRY(STR_STATUS),
	UTIL_NAME_CODER_ENTRY(STR_BLOCK_CATEGORY),
	UTIL_NAME_CODER_ENTRY(STR_STORE_USE),
	UTIL_NAME_CODER_ENTRY(STR_STORE_OBJECT_USE),

};
const util::NameCoder<MetaType::StringConstants, MetaType::END_STR>
		MetaType::Coders::CODER_STR(LIST_STR, 1);


const MetaType::CoreColumns::Entry<MetaType::ContainerMeta>
		MetaType::CoreColumns::COLUMNS_CONTAINER[] = {
	of(CONTAINER_DATABASE_ID).asLong(),
	of(CONTAINER_DATABASE_NAME).asString().asDbName(),
	of(CONTAINER_ATTRIBUTE).asInteger(),
	of(CONTAINER_TYPE_NAME).asString(),
	of(CONTAINER_NAME).asString().asContainerName(),
	of(CONTAINER_DATA_AFFINITY).asString(true),
	of(CONTAINER_EXPIRATION_TIME).asInteger(true),
	of(CONTAINER_EXPIRATION_UNIT).asString(true),
	of(CONTAINER_EXPIRATION_DIVISION).asInteger(true),
	of(CONTAINER_COMPRESSION_METHOD).asString(true),
	of(CONTAINER_COMPRESSION_SIZE).asInteger(true),
	of(CONTAINER_COMPRESSION_UNIT).asString(true),
	of(CONTAINER_PARTITION_TYPE1).asString(true),
	of(CONTAINER_PARTITION_COLUMN1).asString(true),
	of(CONTAINER_PARTITION_INTERVAL1).asString(true),
	of(CONTAINER_PARTITION_UNIT1).asString(true),
	of(CONTAINER_PARTITION_DIVISION1).asInteger(true),
	of(CONTAINER_PARTITION_TYPE2).asString(true),
	of(CONTAINER_PARTITION_COLUMN2).asString(true),
	of(CONTAINER_PARTITION_INTERVAL2).asString(true),
	of(CONTAINER_PARTITION_UNIT2).asString(true),
	of(CONTAINER_PARTITION_DIVISION2).asInteger(true),
	of(CONTAINER_CLUSTER_PARTITION).asInteger(),
	of(CONTAINER_EXPIRATION_TYPE).asString(true)
};
const MetaType::CoreColumns::Entry<MetaType::ColumnMeta>
		MetaType::CoreColumns::COLUMNS_COLUMN[] = {
	of(COLUMN_DATABASE_ID).asLong(),
	of(COLUMN_DATABASE_NAME).asString().asDbName(),
	of(COLUMN_CONTAINER_ATTRIBUTE).asInteger(),
	of(COLUMN_CONTAINER_NAME).asString().asContainerName(),
	of(COLUMN_ORDINAL).asInteger(),
	of(COLUMN_SQL_TYPE).asInteger(),
	of(COLUMN_TYPE_NAME).asString(),
	of(COLUMN_NAME).asString(),
	of(COLUMN_KEY).asBool(),
	of(COLUMN_NULLABLE).asBool(),
	of(COLUMN_KEY_SEQUENCE).asShort(),
	of(COLUMN_COMPRESSION_RELATIVE).asBool(true),
	of(COLUMN_COMPRESSION_RATE).asDouble(true),
	of(COLUMN_COMPRESSION_SPAN).asDouble(true),
	of(COLUMN_COMPRESSION_WIDTH).asDouble(true)
};
const MetaType::CoreColumns::Entry<MetaType::IndexMeta>
		MetaType::CoreColumns::COLUMNS_INDEX[] = {
	of(INDEX_DATABASE_ID).asLong(),
	of(INDEX_DATABASE_NAME).asString().asDbName(),
	of(INDEX_CONTAINER_NAME).asString().asContainerName(),
	of(INDEX_NAME).asString(true),
	of(INDEX_ORDINAL).asShort(),
	of(INDEX_COLUMN_NAME).asString(),
	of(INDEX_TYPE).asShort(),
	of(INDEX_TYPE_NAME).asString()
};
const MetaType::CoreColumns::Entry<MetaType::TriggerMeta>
		MetaType::CoreColumns::COLUMNS_TRIGGER[] = {
	of(TRIGGER_DATABASE_ID).asLong(),
	of(TRIGGER_DATABASE_NAME).asString().asDbName(),
	of(TRIGGER_CONTAINER_NAME).asString().asContainerName(),
	of(TRIGGER_ORDINAL).asShort(),
	of(TRIGGER_NAME).asString(),
	of(TRIGGER_EVENT_TYPE).asString(true),
	of(TRIGGER_COLUMN_NAME).asString(true),
	of(TRIGGER_TYPE).asString(true),
	of(TRIGGER_URI).asString(true),
	of(TRIGGER_JMS_DESTINATION_TYPE).asString(true),
	of(TRIGGER_JMS_DESTINATION_NAME).asString(true),
	of(TRIGGER_USER).asString(true),
	of(TRIGGER_PASSWORD).asString(true)
};

const MetaType::CoreColumns::Entry<MetaType::ErasableMeta>
		MetaType::CoreColumns::COLUMNS_ERASABLE[] = {
	of(ERASABLE_DATABASE_ID).asString(),
	of(ERASABLE_DATABASE_NAME).asString().asDbName(),
	of(ERASABLE_TYPE_NAME).asString(),
	of(ERASABLE_CONTAINER_ID).asString().asContainerId(),
	of(ERASABLE_CONTAINER_NAME).asString(),
	of(ERASABLE_PARTITION_NAME).asString(true),
	of(ERASABLE_CLUSTER_PARTITION).asInteger().asPartitionIndex(),
	of(ERASABLE_LARGE_CONTAINER_ID).asString(true),
	of(ERASABLE_SCHEMA_VERSION_ID).asInteger(),
	of(ERASABLE_INIT_SCHEMA_STATUS).asString(true),
	of(ERASABLE_EXPIRATION_TYPE).asString(),
	of(ERASABLE_LOWER_BOUNDARY_TIME).asTimestamp(),
	of(ERASABLE_UPPER_BOUNDARY_TIME).asTimestamp(),
	of(ERASABLE_EXPIRATION_TIME).asTimestamp(),
	of(ERASABLE_ERASABLE_TIME).asTimestamp(),
	of(ERASABLE_ROW_INDEX_OID).asString(),
	of(ERASABLE_MVCC_INDEX_OID).asString()
};

const MetaType::CoreColumns::Entry<MetaType::EventMeta>
		MetaType::CoreColumns::COLUMNS_EVENT[] = {
	of(EVENT_NODE_ADDRESS).asString(),
	of(EVENT_NODE_PORT).asString().asInteger(),
	of(EVENT_START_TIME).asTimestamp(),
	of(EVENT_APPLICATION_NAME).asString(true),
	of(EVENT_SERVICE_TYPE).asString(),
	of(EVENT_EVENT_TYPE).asString(),
	of(EVENT_WORKER_INDEX).asInteger(),
	of(EVENT_CLUSTER_PARTITION_INDEX).asInteger()
};

const MetaType::CoreColumns::Entry<MetaType::SocketMeta>
		MetaType::CoreColumns::COLUMNS_SOCKET[] = {
	of(SOCKET_SERVICE_TYPE).asString(),
	of(SOCKET_TYPE).asString(true),
	of(SOCKET_NODE_ADDRESS).asString(true),
	of(SOCKET_NODE_PORT).asInteger(true),
	of(SOCKET_REMOTE_ADDRESS).asString(true),
	of(SOCKET_REMOTE_PORT).asInteger(true),
	of(SOCKET_APPLICATION_NAME).asString(true),
	of(SOCKET_CREATION_TIME).asTimestamp(),
	of(SOCKET_DISPATCHING_EVENT_COUNT).asLong(),
	of(SOCKET_SENDING_EVENT_COUNT).asLong()
};

const MetaType::CoreColumns::Entry<MetaType::ContainerStatsMeta>
		MetaType::CoreColumns::COLUMNS_CONTAINER_STATS[] = {
	of(CONTAINER_STATS_DATABASE_ID).asLong(),
	of(CONTAINER_STATS_DATABASE_NAME).asString().asDbName(),
	of(CONTAINER_STATS_NAME).asString().asContainerName(),
	of(CONTAINER_STATS_GROUPID).asLong(true),
	of(CONTAINER_STATS_NUM_ROWS).asLong(true)
};

const MetaType::CoreColumns::Entry<MetaType::ClusterPartitionMeta>
		MetaType::CoreColumns::COLUMNS_CLUSTER_PARTITION[] = {
	of(CLUSTER_PARTITION_CLUSTER_PARTITION_INDEX).asInteger().asPartitionIndex(),
	of(CLUSTER_PARTITION_ROLE).asString(),
	of(CLUSTER_PARTITION_NODE_ADDRESS).asString(),
	of(CLUSTER_PARTITION_NODE_PORT).asInteger(),
	of(CLUSTER_PARTITION_LSN).asLong(),
	of(CLUSTER_PARTITION_STATUS).asString(),
	of(CLUSTER_PARTITION_BLOCK_CATEGORY).asString(),
	of(CLUSTER_PARTITION_STORE_USE).asLong(true),
	of(CLUSTER_PARTITION_STORE_OBJECT_USE).asLong(true),
};

const MetaType::CoreColumns::Entry<MetaType::PartitionMeta>
		MetaType::CoreColumns::COLUMNS_PARTITION[] = {
	of(PARTITION_DATABASE_ID).asLong(),
	of(PARTITION_DATABASE_NAME).asString().asDbName(),
	of(PARTITION_CONTAINER_NAME).asString().asContainerName(),
	of(PARTITION_ORDINAL).asLong(),
	of(PARTITION_NAME).asString(),
	of(PARTITION_BOUNDARY_VALUE1).asString(true),
	of(PARTITION_BOUNDARY_VALUE2).asString(true),
	of(PARTITION_NODE_AFFINITY).asLong(),
	of(PARTITION_CLUSTER_PARTITION_INDEX).asInteger(),
	of(PARTITION_NODE_ADDRESS).asString(),
	of(PARTITION_STATUS).asString()
};


const MetaType::CoreColumns::Entry<MetaType::ViewMeta>
		MetaType::CoreColumns::COLUMNS_VIEW[] = {
	of(VIEW_DATABASE_ID).asLong(),
	of(VIEW_DATABASE_NAME).asString().asDbName(),
	of(VIEW_NAME).asString(),
	of(VIEW_DEFINITION).asString()
};

const MetaType::CoreColumns::Entry<MetaType::SQLMeta>
		MetaType::CoreColumns::COLUMNS_SQL[] = {
	of(SQL_DATABASE_NAME).asString(true),
	of(SQL_NODE_ADDRESS).asString(),
	of(SQL_NODE_PORT).asInteger(),
	of(SQL_START_TIME).asTimestamp(),
	of(SQL_APPLICATION_NAME).asString(true),
	of(SQL_SQL).asString(true),
	of(SQL_QUERY_ID).asString(),
	of(SQL_JOB_ID).asString(true)
};

const MetaType::CoreColumns::Entry<MetaType::PartitionStatsMeta>
		MetaType::CoreColumns::COLUMNS_PARTITION_STATS[] = {
	of(PARTITION_STATS_DATABASE_ID).asLong(),
	of(PARTITION_STATS_DATABASE_NAME).asString().asDbName(),
	of(PARTITION_STATS_CONTAINER_NAME).asString().asContainerName(),
	of(PARTITION_STATS_NAME).asString(),
	of(PARTITION_STATS_NUM_ROWS).asLong(true)
};



template<typename T>
MetaType::CoreColumns::Entry<T> MetaType::CoreColumns::of(T id) {
	return Entry<T>(id);
}

template<typename T>
MetaType::CoreColumns::Entry<T>::Entry(T id) :
		id_(id),
		type_(COLUMN_TYPE_NULL),
		nullable_(false),
		common_(END_COMMON) {
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asType(uint8_t type, bool nullable) const {
	Entry dest = *this;
	dest.type_ = type;
	dest.nullable_ = nullable;
	return dest;
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asCommon(CommonMetaType common) const {
	Entry dest = *this;
	dest.common_ = common;
	return dest;
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asString(bool nullable) const {
	return asType(COLUMN_TYPE_STRING, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asBool(bool nullable) const {
	return asType(COLUMN_TYPE_BOOL, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asShort(bool nullable) const {
	return asType(COLUMN_TYPE_SHORT, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asInteger(bool nullable) const {
	return asType(COLUMN_TYPE_INT, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asLong(bool nullable) const {
	return asType(COLUMN_TYPE_LONG, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asDouble(bool nullable) const {
	return asType(COLUMN_TYPE_DOUBLE, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asTimestamp(bool nullable) const {
	return asType(COLUMN_TYPE_TIMESTAMP, nullable);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asDbName() const {
	return asCommon(COMMON_DATABASE_NAME);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asContainerName() const {
	return asCommon(COMMON_CONTAINER_NAME);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asPartitionIndex() const {
	return asCommon(COMMON_PARTITION_INDEX);
}

template<typename T> MetaType::CoreColumns::Entry<T>
MetaType::CoreColumns::Entry<T>::asContainerId() const {
	return asCommon(COMMON_CONTAINER_ID);
}


const MetaType::RefColumns::Entry<MetaType::ContainerMeta>
		MetaType::RefColumns::COLUMNS_CONTAINER[] = {
	of(CONTAINER_DATABASE_NAME, STR_DATABASE_NAME),
	of(CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(CONTAINER_TYPE_NAME,
			STR_CONTAINER_OPTIONAL_TYPE,
			STR_TABLE_OPTIONAL_TYPE),
	of(CONTAINER_DATA_AFFINITY, STR_DATA_AFFINITY),

	of(CONTAINER_EXPIRATION_TIME, STR_EXPIRATION_TIME),
	of(CONTAINER_EXPIRATION_UNIT, STR_EXPIRATION_TIME_UNIT),
	of(CONTAINER_EXPIRATION_DIVISION, STR_EXPIRATION_DIVISION_COUNT),

	of(CONTAINER_COMPRESSION_METHOD, STR_COMPRESSION_METHOD),
	of(CONTAINER_COMPRESSION_SIZE, STR_COMPRESSION_WINDOW_SIZE),
	of(CONTAINER_COMPRESSION_UNIT, STR_COMPRESSION_WINDOW_SIZE_UNIT),

	of(CONTAINER_PARTITION_TYPE1, STR_PARTITION_TYPE),
	of(CONTAINER_PARTITION_COLUMN1, STR_PARTITION_COLUMN),
	of(CONTAINER_PARTITION_INTERVAL1, STR_PARTITION_INTERVAL_VALUE),
	of(CONTAINER_PARTITION_UNIT1, STR_PARTITION_INTERVAL_UNIT),
	of(CONTAINER_PARTITION_DIVISION1, STR_PARTITION_DIVISION_COUNT),

	of(CONTAINER_PARTITION_TYPE2, STR_SUBPARTITION_TYPE),
	of(CONTAINER_PARTITION_COLUMN2, STR_SUBPARTITION_COLUMN),
	of(CONTAINER_PARTITION_INTERVAL2, STR_SUBPARTITION_INTERVAL_VALUE),
	of(CONTAINER_PARTITION_UNIT2, STR_SUBPARTITION_INTERVAL_UNIT),
	of(CONTAINER_PARTITION_DIVISION2, STR_SUBPARTITION_DIVISION_COUNT),

	of(CONTAINER_CLUSTER_PARTITION, STR_CLUSTER_PARTITION_INDEX),
	of(CONTAINER_EXPIRATION_TYPE, STR_EXPIRATION_TYPE)
};
const MetaType::RefColumns::Entry<MetaType::ColumnMeta>
		MetaType::RefColumns::COLUMNS_COLUMN[] = {
	of(COLUMN_DATABASE_NAME, STR_DATABASE_NAME),
	of(COLUMN_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(COLUMN_ORDINAL, STR_ORDINAL_POSITION),
	of(COLUMN_NAME, STR_COLUMN_NAME),
	of(COLUMN_TYPE_NAME, STR_TYPE_NAME),
	of(COLUMN_NULLABLE, STR_NULLABLE),
	of(COLUMN_COMPRESSION_RELATIVE, STR_COMPRESSION_RELATIVE),
	of(COLUMN_COMPRESSION_RATE, STR_COMPRESSION_RATE),
	of(COLUMN_COMPRESSION_SPAN, STR_COMPRESSION_SPAN),
	of(COLUMN_COMPRESSION_WIDTH, STR_COMPRESSION_WIDTH)
};
const MetaType::RefColumns::Entry<MetaType::ColumnMeta>
		MetaType::RefColumns::COLUMNS_KEY[] = {
	of(COLUMN_DATABASE_NAME, STR_DATABASE_NAME),
	of(COLUMN_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(COLUMN_NAME, STR_COLUMN_NAME),
	of(COLUMN_KEY_SEQUENCE, STR_KEY_SEQ)
};
const MetaType::RefColumns::Entry<MetaType::IndexMeta>
		MetaType::RefColumns::COLUMNS_INDEX[] = {
	of(INDEX_DATABASE_NAME, STR_DATABASE_NAME),
	of(INDEX_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(INDEX_NAME, STR_INDEX_NAME),
	of(INDEX_TYPE_NAME, STR_INDEX_TYPE),
	of(INDEX_ORDINAL, STR_ORDINAL_POSITION),
	of(INDEX_COLUMN_NAME, STR_COLUMN_NAME)
};
const MetaType::RefColumns::Entry<MetaType::TriggerMeta>
		MetaType::RefColumns::COLUMNS_TRIGGER[] = {
	of(TRIGGER_DATABASE_NAME, STR_DATABASE_NAME),
	of(TRIGGER_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(TRIGGER_ORDINAL, STR_ORDINAL_POSITION),
	of(TRIGGER_NAME, STR_TRIGGER_NAME),
	of(TRIGGER_EVENT_TYPE, STR_EVENT_TYPE),
	of(TRIGGER_COLUMN_NAME, STR_COLUMN_NAME),
	of(TRIGGER_TYPE, STR_TRIGGER_TYPE),
	of(TRIGGER_URI, STR_URI),
	of(TRIGGER_JMS_DESTINATION_TYPE, STR_JMS_DESTINATION_TYPE),
	of(TRIGGER_JMS_DESTINATION_NAME, STR_JMS_DESTINATION_NAME),
	of(TRIGGER_USER, STR_USER),
	of(TRIGGER_PASSWORD, STR_PASSWORD)
};

const MetaType::RefColumns::Entry<MetaType::ErasableMeta>
		MetaType::RefColumns::COLUMNS_ERASABLE[] = {
	of(ERASABLE_DATABASE_NAME, STR_DATABASE_NAME),
	of(ERASABLE_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(ERASABLE_PARTITION_NAME, STR_PARTITION_NAME),
	of(ERASABLE_TYPE_NAME, STR_TABLE_OPTIONAL_TYPE,
			STR_CONTAINER_OPTIONAL_TYPE),
	of(ERASABLE_EXPIRATION_TYPE, STR_EXPIRATION_TYPE),
	of(ERASABLE_DATABASE_ID, STR_DATABASE_ID),
	of(ERASABLE_CONTAINER_ID, STR_CONTAINER_ID),
	of(ERASABLE_CLUSTER_PARTITION, STR_CLUSTER_PARTITION_INDEX),
	of(ERASABLE_LARGE_CONTAINER_ID, STR_LARGE_CONTAINER_ID),
	of(ERASABLE_SCHEMA_VERSION_ID, STR_SCHEMA_VERSION_ID),
	of(ERASABLE_INIT_SCHEMA_STATUS, STR_INIT_SCHEMA_STATUS),
	of(ERASABLE_LOWER_BOUNDARY_TIME, STR_LOWER_BOUNDARY_TIME),
	of(ERASABLE_UPPER_BOUNDARY_TIME, STR_UPPER_BOUNDARY_TIME),
	of(ERASABLE_EXPIRATION_TIME, STR_EXPIRATION_TIME),
	of(ERASABLE_ERASABLE_TIME, STR_ERASABLE_TIME),
	of(ERASABLE_ROW_INDEX_OID, STR_ROW_INDEX_OID),
	of(ERASABLE_MVCC_INDEX_OID, STR_MVCC_INDEX_OID)
};

const MetaType::RefColumns::Entry<MetaType::EventMeta>
		MetaType::RefColumns::COLUMNS_EVENT[] = {
	of(EVENT_NODE_ADDRESS, STR_NODE_ADDRESS),
	of(EVENT_NODE_PORT, STR_NODE_PORT),
	of(EVENT_START_TIME, STR_START_TIME),
	of(EVENT_APPLICATION_NAME, STR_APPLICATION_NAME),
	of(EVENT_SERVICE_TYPE, STR_SERVICE_TYPE),
	of(EVENT_EVENT_TYPE, STR_EVENT_TYPE),
	of(EVENT_WORKER_INDEX, STR_WORKER_INDEX),
	of(EVENT_CLUSTER_PARTITION_INDEX, STR_CLUSTER_PARTITION_INDEX)
};

const MetaType::RefColumns::Entry<MetaType::SocketMeta>
		MetaType::RefColumns::COLUMNS_SOCKET[] = {
	of(SOCKET_SERVICE_TYPE, STR_SERVICE_TYPE),
	of(SOCKET_TYPE, STR_SOCKET_TYPE),
	of(SOCKET_NODE_ADDRESS, STR_NODE_ADDRESS),
	of(SOCKET_NODE_PORT, STR_NODE_PORT),
	of(SOCKET_REMOTE_ADDRESS, STR_REMOTE_ADDRESS),
	of(SOCKET_REMOTE_PORT, STR_REMOTE_PORT),
	of(SOCKET_APPLICATION_NAME, STR_APPLICATION_NAME),
	of(SOCKET_CREATION_TIME, STR_CREATION_TIME),
	of(SOCKET_DISPATCHING_EVENT_COUNT, STR_DISPATCHING_EVENT_COUNT),
	of(SOCKET_SENDING_EVENT_COUNT, STR_SENDING_EVENT_COUNT)
};

const MetaType::RefColumns::Entry<MetaType::ContainerStatsMeta>
		MetaType::RefColumns::COLUMNS_CONTAINER_STATS[] = {
	of(CONTAINER_STATS_DATABASE_NAME, STR_DATABASE_NAME),
	of(CONTAINER_STATS_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(CONTAINER_STATS_GROUPID, STR_DATA_GROUPID),
	of(CONTAINER_STATS_NUM_ROWS, STR_NUM_ROWS)
};

const MetaType::RefColumns::Entry<MetaType::ClusterPartitionMeta>
		MetaType::RefColumns::COLUMNS_CLUSTER_PARTITION[] = {
	of(CLUSTER_PARTITION_CLUSTER_PARTITION_INDEX, STR_CLUSTER_PARTITION_INDEX),
	of(CLUSTER_PARTITION_ROLE, STR_ROLE),
	of(CLUSTER_PARTITION_NODE_ADDRESS, STR_NODE_ADDRESS),
	of(CLUSTER_PARTITION_NODE_PORT, STR_NODE_PORT),
	of(CLUSTER_PARTITION_LSN, STR_LSN),
	of(CLUSTER_PARTITION_STATUS, STR_STATUS),
	of(CLUSTER_PARTITION_BLOCK_CATEGORY, STR_BLOCK_CATEGORY),
	of(CLUSTER_PARTITION_STORE_USE, STR_STORE_USE),
	of(CLUSTER_PARTITION_STORE_OBJECT_USE, STR_STORE_OBJECT_USE),
};

const MetaType::RefColumns::Entry<MetaType::PartitionMeta>
		MetaType::RefColumns::COLUMNS_PARTITION[] = {
	of(PARTITION_DATABASE_NAME, STR_DATABASE_NAME),
	of(PARTITION_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(PARTITION_ORDINAL, STR_PARTITION_SEQ),
	of(PARTITION_NAME, STR_PARTITION_NAME),

	of(PARTITION_BOUNDARY_VALUE1, STR_PARTITION_BOUNDARY_VALUE),
	of(PARTITION_BOUNDARY_VALUE2, STR_SUBPARTITION_BOUNDARY_VALUE),
	of(PARTITION_NODE_AFFINITY, STR_PARTITION_NODE_AFFINITY),
	of(PARTITION_CLUSTER_PARTITION_INDEX, STR_CLUSTER_PARTITION_INDEX)

	,
	of(PARTITION_NODE_ADDRESS, STR_CLUSTER_NODE_ADDRESS),
	of(PARTITION_STATUS, STR_PARTITION_STATUS)
};

const MetaType::RefColumns::Entry<MetaType::ViewMeta>
		MetaType::RefColumns::COLUMNS_VIEW[] = {
	of(VIEW_DATABASE_NAME, STR_DATABASE_NAME),
	of(VIEW_NAME, STR_VIEW_NAME),
	of(VIEW_DEFINITION, STR_VIEW_DEFINITION)
};

const MetaType::RefColumns::Entry<MetaType::SQLMeta>
		MetaType::RefColumns::COLUMNS_SQL[] = {
	of(SQL_DATABASE_NAME, STR_DATABASE_NAME),
	of(SQL_NODE_ADDRESS, STR_NODE_ADDRESS),
	of(SQL_NODE_PORT, STR_NODE_PORT),
	of(SQL_START_TIME, STR_START_TIME),
	of(SQL_APPLICATION_NAME, STR_APPLICATION_NAME),
	of(SQL_SQL, STR_SQL),
	of(SQL_QUERY_ID, STR_QUERY_ID),
	of(SQL_JOB_ID, STR_JOB_ID)
};

const MetaType::RefColumns::Entry<MetaType::PartitionStatsMeta>
		MetaType::RefColumns::COLUMNS_PARTITION_STATS[] = {
	of(PARTITION_STATS_DATABASE_NAME, STR_DATABASE_NAME),
	of(PARTITION_STATS_CONTAINER_NAME, STR_CONTAINER_NAME, STR_TABLE_NAME),
	of(PARTITION_STATS_NAME, STR_PARTITION_NAME),
	of(PARTITION_STATS_NUM_ROWS, STR_NUM_ROWS)
};

template<typename T>
MetaType::RefColumns::Entry<T> MetaType::RefColumns::of(
		T refId,
		StringConstants nameForContainer, StringConstants nameForTable) {
	return Entry<T>(refId, nameForContainer, nameForTable);
}

template<typename T>
MetaType::RefColumns::Entry<T>::Entry(
		T refId,
		StringConstants nameForContainer, StringConstants nameForTable) :
		refId_(refId),
		nameForContainer_(nameForContainer),
		nameForTable_(nameForTable) {
}


const MetaContainerInfo MetaType::Containers::CONTAINERS_CORE[] = {
	coreOf(
			TYPE_CONTAINER, "_core_containers",
			CoreColumns::COLUMNS_CONTAINER, Coders::CODER_CONTAINER, 0),
	coreOf(
			TYPE_COLUMN, "_core_columns",
			CoreColumns::COLUMNS_COLUMN, Coders::CODER_COLUMN, 0),
	noneOf(TYPE_KEY),
	coreOf(
			TYPE_INDEX, "_core_index_info",
			CoreColumns::COLUMNS_INDEX, Coders::CODER_INDEX, 0),
	coreOf(
			TYPE_TRIGGER, "_core_event_triggers",
			CoreColumns::COLUMNS_TRIGGER, Coders::CODER_TRIGGER, 0),
	coreOf(
			TYPE_ERASABLE, "_core_internal_erasables",
			CoreColumns::COLUMNS_ERASABLE, Coders::CODER_ERASABLE, 0),
	toNodeDistribution(coreOf(
			TYPE_EVENT, "_core_events",
			CoreColumns::COLUMNS_EVENT, Coders::CODER_EVENT, 0)),
	toNodeDistribution(coreOf(
			TYPE_SOCKET, "_core_sockets",
			CoreColumns::COLUMNS_SOCKET, Coders::CODER_SOCKET, 0)),

	coreOf(
			TYPE_CONTAINER_STATS, "_core_containers_stats",
			CoreColumns::COLUMNS_CONTAINER_STATS, Coders::CODER_CONTAINER_STATS, 0),
	coreOf(
			TYPE_CLUSTER_PARTITION, "_core_cluster_partitions",
			CoreColumns::COLUMNS_CLUSTER_PARTITION, Coders::CODER_CLUSTER_PARTITION, 0)
	,
	coreOf(
			TYPE_PARTITION, "_core_table_partitions",
			CoreColumns::COLUMNS_PARTITION, Coders::CODER_PARTITION, 0),
	coreOf(
			TYPE_VIEW, "_core_views",
			CoreColumns::COLUMNS_VIEW, Coders::CODER_VIEW, 0),
	toNodeDistribution(coreOf(
			TYPE_SQL, "_core_sqls",
			CoreColumns::COLUMNS_SQL, Coders::CODER_SQL, 0)),

	coreOf(
			TYPE_PARTITION_STATS, "_core_table_partitions_stats",
			CoreColumns::COLUMNS_PARTITION_STATS, Coders::CODER_PARTITION_STATS, 0),
};

const MetaContainerInfo MetaType::Containers::CONTAINERS_REF[] = {
	refOf(TYPE_CONTAINER, NULL, "containers", "tables",
			RefColumns::COLUMNS_CONTAINER, 0),
	refOf(TYPE_COLUMN,
			"columns", "container_columns", "table_columns",
			RefColumns::COLUMNS_COLUMN, 0),
	refOf(TYPE_KEY,
			"primary_keys", "container_primary_keys", "table_primary_keys",
			RefColumns::COLUMNS_KEY, 0, TYPE_COLUMN),
	refOf(TYPE_INDEX,
			"index_info", "container_index_info", "table_index_info",
			RefColumns::COLUMNS_INDEX, 0),
	refOf(TYPE_TRIGGER,
			"event_triggers", "container_event_triggers", "table_event_triggers",
			RefColumns::COLUMNS_TRIGGER, 0),
	toInternal(refOf(
			TYPE_ERASABLE,
			"_internal_erasables",
			"_internal_container_erasables", "_internal_table_erasables",
			RefColumns::COLUMNS_ERASABLE, 0)),
	refOf(TYPE_EVENT,
			"events", "container_events", "table_events",
			RefColumns::COLUMNS_EVENT, 0),
	refOf(TYPE_SOCKET,
			"sockets", NULL, NULL, RefColumns::COLUMNS_SOCKET, 0),
	refOf(TYPE_CONTAINER_STATS, NULL, "containers_stats", "tables_stats",
			RefColumns::COLUMNS_CONTAINER_STATS, 0),
	refOf(TYPE_CLUSTER_PARTITION,
			"cluster_partitions", NULL, NULL, RefColumns::COLUMNS_CLUSTER_PARTITION, 0)

	,
	refOf(TYPE_PARTITION,
			NULL, "container_partitions", "table_partitions",
			RefColumns::COLUMNS_PARTITION, 0),
	refOf(TYPE_VIEW,
			"views", "container_views", "table_views",
			RefColumns::COLUMNS_VIEW, 0),
	refOf(TYPE_SQL,
			"sqls", "container_sqls", "table_sqls",
			RefColumns::COLUMNS_SQL, 0)	,
	refOf(TYPE_PARTITION_STATS,
			NULL, "container_partitions_stats", "table_partitions_stats",
			RefColumns::COLUMNS_PARTITION_STATS, 0),
};

MetaContainerId MetaType::Containers::typeToId(MetaContainerType type) {
	if (0 <= type && type < END_TYPE) {
		return static_cast<MetaContainerId>(type);
	}
	else if (START_TYPE_SQL <= type && type < END_TYPE_SQL) {
		return static_cast<MetaContainerId>(type);
	}
	else {
		return UNDEF_CONTAINERID;
	}
}

bool MetaType::Containers::idToIndex(MetaContainerId id, size_t &index) {
	if (id < END_TYPE) {
		index = static_cast<size_t>(id);
		return true;
	}
	else if (START_TYPE_SQL <= id && id < END_TYPE_SQL) {
		index = static_cast<size_t>(id - START_TYPE_SQL) + END_TYPE;
		return true;
	}
	else {
		index = std::numeric_limits<size_t>::max();
		return false;
	}
}

template<typename T, size_t N>
MetaContainerInfo MetaType::Containers::coreOf(
		MetaContainerType type, const char8_t *name,
		const CoreColumns::Entry<T> (&columnList)[N],
		const util::NameCoder<T, N> &coder, uint16_t version) {
	static MetaColumnInfo destColumnList[N];

	MetaContainerInfo::CommonColumnInfo commonInfo;
	for (size_t i = 0; i < N; i++) {
		const CoreColumns::Entry<T> &srcInfo = columnList[i];
		MetaColumnInfo &destInfo = destColumnList[i];

		setUpCoreColumnInfo(
				i, srcInfo.id_, srcInfo.type_, srcInfo.nullable_,
				coder(srcInfo.id_), destInfo);
		setUpCommonColumnInfo(srcInfo.id_, srcInfo.common_, commonInfo);
	}

	{
		MetaContainerInfo info;
		info.id_ = typeToId(type);
		info.versionId_ = makeSchemaVersionId(version, N);
		info.forCore_ = true;
		info.name_.neutral_ = name;
		info.columnList_ = destColumnList;
		info.columnCount_ = N;
		info.commonInfo_ = commonInfo;
		return info;
	}
}

template<typename T, size_t N>
MetaContainerInfo MetaType::Containers::refOf(
		MetaContainerType type, const char8_t *name,
		const char8_t *nameForContainer, const char8_t *nameForTable,
		const RefColumns::Entry<T> (&columnList)[N], uint16_t version,
		MetaContainerType refType) {
	static MetaColumnInfo destColumnList[N];

	const MetaContainerType resolvedRefType =
			(refType == END_TYPE ? type : refType);
	for (size_t i = 0; i < N; i++) {
		const RefColumns::Entry<T> &srcInfo = columnList[i];
		MetaColumnInfo &destInfo = destColumnList[i];

		setUpRefColumnInfo(
				resolvedRefType, i, srcInfo.refId_, srcInfo.nameForContainer_,
				srcInfo.nameForTable_, destInfo);
	}

	{
		size_t coreIndex;
		idToIndex(resolvedRefType, coreIndex);

		MetaContainerInfo info;
		info.id_ = typeToId(type);
		info.refId_ = typeToId(resolvedRefType);
		info.versionId_ = makeSchemaVersionId(version, N);
		info.forCore_ = false;
		info.nodeDistribution_ =
				CONTAINERS_CORE[coreIndex].nodeDistribution_;
		info.name_.neutral_ = name;
		info.name_.forContainer_ = nameForContainer;
		info.name_.forTable_ = nameForTable;
		info.columnList_ = destColumnList;
		info.columnCount_ = N;
		return info;
	}
}

MetaContainerInfo MetaType::Containers::noneOf(MetaContainerType type) {
	MetaContainerInfo info;
	info.id_ = typeToId(type);
	return info;
}

MetaContainerInfo MetaType::Containers::toInternal(
		const MetaContainerInfo &src) {
	MetaContainerInfo info = src;
	info.internal_ = true;
	return info;
}

MetaContainerInfo MetaType::Containers::toNodeDistribution(
		const MetaContainerInfo &src) {
	MetaContainerInfo info = src;
	info.nodeDistribution_ = true;
	return info;
}

void MetaType::Containers::setUpCoreColumnInfo(
		size_t index, ColumnId id, uint8_t type, bool nullable,
		const char8_t *name, MetaColumnInfo &info) {
	assert(info.id_ == UNDEF_COLUMNID);
	assert(index == id);
	assert(name != NULL);

	info.id_ = id;
	info.type_ = type;
	info.nullable_ = nullable;
	info.name_.forContainer_ = name;
}

void MetaType::Containers::setUpRefColumnInfo(
		MetaContainerType type, size_t index, ColumnId refId,
		StringConstants nameForContainer, StringConstants nameForTable,
		MetaColumnInfo &info) {
	size_t typeIndex;
	if (!idToIndex(typeToId(type), typeIndex)) {
		assert(false);
		return;
	}
	assert(typeIndex <
			sizeof(CONTAINERS_CORE) / sizeof(*CONTAINERS_CORE));
	const MetaContainerInfo &refContainerInfo = CONTAINERS_CORE[typeIndex];

	assert(refId < refContainerInfo.columnCount_);
	const MetaColumnInfo &refInfo = refContainerInfo.columnList_[refId];
	assert(refInfo.id_ != UNDEF_COLUMNID);

	const char8_t *nameStrForContainer = Coders::CODER_STR(nameForContainer);
	const char8_t *nameStrForTable = Coders::CODER_STR(nameForTable);

	assert(info.id_ == UNDEF_COLUMNID);
	assert(nameStrForContainer != NULL);

	info.id_ = static_cast<ColumnId>(index);
	info.refId_ = refId;
	info.type_ = refInfo.type_;
	info.nullable_ = refInfo.nullable_;
	info.name_.forContainer_ = nameStrForContainer;
	info.name_.forTable_ = nameStrForTable;
}

void MetaType::Containers::setUpCommonColumnInfo(
		ColumnId id, CommonMetaType type,
		MetaContainerInfo::CommonColumnInfo &commonInfo) {
	switch (type) {
	case COMMON_DATABASE_NAME:
		commonInfo.dbNameColumn_ = id;
		break;
	case COMMON_CONTAINER_NAME:
		commonInfo.containerNameColumn_ = id;
		break;
	case COMMON_PARTITION_INDEX:
		commonInfo.partitionIndexColumn_ = id;
		break;
	case COMMON_CONTAINER_ID:
		commonInfo.containerIdColumn_ = id;
		break;
	default:
		break;
	}
}

SchemaVersionId MetaType::Containers::makeSchemaVersionId(
		uint16_t base, size_t columnCount) {
	const SchemaVersionId shiftBits = (CHAR_BIT * sizeof(uint16_t));
	assert(columnCount < (1U << shiftBits));

	return (base << shiftBits) | static_cast<uint16_t>(columnCount);
}


const MetaType::InfoTable MetaType::InfoTable::TABLE_INSTANCE;

const MetaType::InfoTable& MetaType::InfoTable::getInstance() {
	return TABLE_INSTANCE;
}

const MetaContainerInfo& MetaType::InfoTable::resolveInfo(
		MetaContainerId id, bool forCore) const {
	const MetaContainerInfo *info = findInfo(id, forCore);
	if (info == NULL) {
		GS_THROW_USER_ERROR(GS_ERROR_DS_CONTAINER_NOT_FOUND, "");
	}
	return *info;
}

const MetaContainerInfo* MetaType::InfoTable::findInfo(
		const char8_t *name, bool forCore, NamingType &namingType) const {
	assert(name != NULL);
	namingType = NAMING_NEUTRAL;

	const NameEntry *entryList = (forCore ? coreList_ : refList_);
	const NameEntry *entryEnd = entryList + (forCore ?
			getNameEntryCount(coreList_) : getNameEntryCount(refList_));
	const NameEntry key(name, NameEntry::second_type());
	const std::pair<const NameEntry*, const NameEntry*> &range =
			std::equal_range<const NameEntry*>(
					entryList, entryEnd, key, EntryPred());

	if (range.first == range.second) {
		return NULL;
	}

	const EntryValue &entryValue = range.first->second;

	const MetaContainerInfo *info = findInfoByIndex(
			static_cast<size_t>(entryValue.first), forCore);
	if (info == NULL) {
		return NULL;
	}

	namingType = entryValue.second;
	return info;
}

const MetaContainerInfo* MetaType::InfoTable::findInfo(
		MetaContainerId id, bool forCore) const {
	size_t index;
	if (!Containers::idToIndex(id, index)) {
		return NULL;
	}

	return findInfoByIndex(index, forCore);
}

MetaType::NamingType MetaType::InfoTable::resolveNaming(
		NamingType specifiedType, NamingType defaultType) {
	if (specifiedType == NAMING_NEUTRAL) {
		return defaultType;
	}
	else {
		return specifiedType;
	}
}

const char8_t* MetaType::InfoTable::nameOf(
		const MetaColumnInfo &columnInfo, NamingType namingType,
		bool exact) {
	const char8_t *name;
	if (namingType == NAMING_TABLE) {
		name = columnInfo.name_.forTable_;
	}
	else {
		name = columnInfo.name_.forContainer_;
	}

	if (name == NULL && !exact) {
		name = columnInfo.name_.forContainer_;
		assert(name != NULL);
	}

	return name;
}

const char8_t* MetaType::InfoTable::nameOf(
		const MetaContainerInfo &containerInfo, NamingType namingType,
		bool exact) {
	const char8_t *name;
	if (namingType == NAMING_TABLE) {
		name = containerInfo.name_.forTable_;
	}
	else if (namingType == NAMING_CONTAINER) {
		name = containerInfo.name_.forContainer_;
	}
	else {
		name = containerInfo.name_.neutral_;
	}

	if (name == NULL && !exact) {
		if (namingType == NAMING_NEUTRAL) {
			name = containerInfo.name_.forContainer_;
		}
		else {
			name = containerInfo.name_.neutral_;
		}
	}

	return name;
}

MetaType::InfoTable::InfoTable() {
	setUpNameEntries(Containers::CONTAINERS_CORE, coreList_);
	setUpNameEntries(Containers::CONTAINERS_REF, refList_);
}

const MetaContainerInfo* MetaType::InfoTable::findInfoByIndex(
		size_t index, bool forCore) {
	if (index >= Containers::TYPE_COUNT) {
		assert(false);
		return NULL;
	}

	const MetaContainerInfo *info;
	if (forCore) {
		info = &Containers::CONTAINERS_CORE[index];
	}
	else {
		info = &Containers::CONTAINERS_REF[index];
	}

	if (info->isEmpty()) {
		return NULL;
	}

	return info;
}

template<size_t N, size_t M>
void MetaType::InfoTable::setUpNameEntries(
		const MetaContainerInfo (&infoList)[N], NameEntry (&entryList)[M]) {
	const size_t namingCount = M / N;
	const bool forCore = (namingCount == 1);
	UTIL_STATIC_ASSERT(N * namingCount == M);
	UTIL_STATIC_ASSERT(forCore || namingCount == END_NAMING);

	for (size_t i = 0; i < N; i++) {
		const MetaContainerInfo &info = infoList[i];
		assert(info.isEmpty() || !info.forCore_ == !forCore);

		if (forCore) {
			assert(!info.internal_);
		}

		size_t expectedIndex;
		Containers::idToIndex(info.id_, expectedIndex);
		assert(i == expectedIndex);

		for (size_t j = 0; j < namingCount; j++) {
			NameEntry &entry = entryList[namingCount * i + j];
			const NamingType namingType = static_cast<NamingType>(j);

			const char8_t *name =
					(info.isEmpty() ? NULL : nameOf(info, namingType, true));
			entry.first = (name == NULL ? "" : name);
			entry.second.first = i;
			entry.second.second = namingType;
		}
	}
	std::sort(entryList, entryList + M, EntryPred());
}

template<size_t M>
size_t MetaType::InfoTable::getNameEntryCount(const NameEntry (&)[M]) {
	return M;
}

bool MetaType::InfoTable::EntryPred::operator()(
		const NameEntry &entry1, const NameEntry &entry2) const {
	return util::stricmp(entry1.first, entry2.first) < 0;
}


MetaProcessor::MetaProcessor(
		TransactionContext &txn, MetaContainerId id, bool forCore) :
		info_(MetaType::InfoTable::getInstance().resolveInfo(id, forCore)),
		nextContainerId_(0),
		containerLimit_(std::numeric_limits<uint64_t>::max()),
		containerKey_(NULL) {
	static_cast<void>(txn);
}

void MetaProcessor::scan(
		TransactionContext &txn, const Source &source, RowHandler &handler) {
	if (info_.forCore_) {
		Context cxt(txn, *this, source, handler);
		switch (info_.id_) {
		case MetaType::TYPE_CONTAINER:
			scanCore<ContainerHandler>(txn, cxt);
			break;
		case MetaType::TYPE_COLUMN:
			scanCore<ColumnHandler>(txn, cxt);
			break;
		case MetaType::TYPE_INDEX:
			scanCore<IndexHandler>(txn, cxt);
			break;
		case MetaType::TYPE_TRIGGER:
			scanCore<MetaTriggerHandler>(txn, cxt);
			break;
		case MetaType::TYPE_ERASABLE:
			scanCore<ErasableHandler>(txn, cxt);
			break;
		case MetaType::TYPE_EVENT: 
			scanCore<EventHandler>(txn, cxt);
			break;
		case MetaType::TYPE_SOCKET: 
			scanCore<SocketHandler>(txn, cxt);
			break;
		case MetaType::TYPE_CONTAINER_STATS:
			scanCore<ContainerStatsHandler>(txn, cxt);
			break;
		case MetaType::TYPE_CLUSTER_PARTITION: 
			scanCore<ClusterPartitionHandler>(txn, cxt);
			break;

		case MetaType::TYPE_PARTITION:
			scanCore<PartitionHandler>(txn, cxt);
			break;
		case MetaType::TYPE_VIEW: 
			scanCore<ViewHandler>(txn, cxt);
			break;
		case MetaType::TYPE_SQL: 
			scanCore<SQLHandler>(txn, cxt);
			break;
		case MetaType::TYPE_PARTITION_STATS:
			scanCore<PartitionStatsHandler>(txn, cxt);
			break;
		default:
			GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
		}
	}
	else {
		MetaProcessor subProcessor(txn, info_.refId_, true);
		subProcessor.setState(*this);
		assert(subProcessor.info_.forCore_);

		if (info_.id_ == MetaType::TYPE_KEY) {
			KeyRefHandler refHandler(txn, info_, handler);
			subProcessor.scan(txn, source, refHandler);
		}
		else if (info_.id_ == MetaType::TYPE_CONTAINER) {
			ContainerRefHandler refHandler(txn, info_, handler);
			subProcessor.scan(txn, source, refHandler);
		}
		else {
			RefHandler refHandler(txn, info_, handler);
			subProcessor.scan(txn, source, refHandler);
		}

		setState(subProcessor);
	}
}

bool MetaProcessor::isSuspended() const {
	return (nextContainerId_ != UNDEF_CONTAINERID);
}

ContainerId MetaProcessor::getNextContainerId() const {
	return nextContainerId_;
}

void MetaProcessor::setNextContainerId(ContainerId containerId) {
	nextContainerId_ = containerId;
}

void MetaProcessor::setContainerLimit(uint64_t limit) {
	containerLimit_ = limit;
}

void MetaProcessor::setContainerKey(const FullContainerKey *containerKey) {
	containerKey_ = containerKey;
}

template<typename HandlerType>
void MetaProcessor::scanCore(TransactionContext &txn, Context &cxt) {
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	KeyDataStore::ContainerCondition condition(alloc);

	const bool singleRequired = (info_.id_ != MetaType::TYPE_PARTITION);
	const bool largeRequired = (info_.id_ != MetaType::TYPE_ERASABLE);
	const bool subRequired = ((info_.id_ == MetaType::TYPE_ERASABLE)
		|| (info_.id_ == MetaType::TYPE_PARTITION_STATS));
	const bool viewRequired = ((info_.id_ == MetaType::TYPE_VIEW)
							   || (info_.id_ == MetaType::TYPE_CONTAINER)); 

	if (info_.nodeDistribution_) {
		HandlerType handler(cxt);
		DataStoreV4 *dataStore = cxt.getSource().dataStore_;
		BaseContainer *container = ALLOC_NEW(txn.getDefaultAllocator()) Collection(txn, dataStore);
		handler(
				txn, UNDEF_CONTAINERID, UNDEF_DBID,
				CONTAINER_ATTR_ANY, *container);
		setNextContainerId(UNDEF_CONTAINERID);
		ALLOC_DELETE(txn.getDefaultAllocator(), container);
		return;
	}
	bool isPartitionUnit = false;
	isPartitionUnit = (isPartitionUnit || (info_.id_ == MetaType::TYPE_CLUSTER_PARTITION));
	if (isPartitionUnit) {
		HandlerType handler(cxt);
		DataStoreV4 *dataStore = cxt.getSource().dataStore_;
		BaseContainer *container = ALLOC_NEW(txn.getDefaultAllocator()) Collection(txn, dataStore);
		handler(
				txn, UNDEF_CONTAINERID, UNDEF_DBID,
				CONTAINER_ATTR_ANY, *container);
		setNextContainerId(UNDEF_CONTAINERID);
		ALLOC_DELETE(txn.getDefaultAllocator(), container);
		return;
	}

	if (singleRequired) {
		condition.insertAttribute(CONTAINER_ATTR_SINGLE);
	}
	if (largeRequired) {
		condition.insertAttribute(CONTAINER_ATTR_LARGE);
	}
	if (subRequired) {
		condition.insertAttribute(CONTAINER_ATTR_SUB);
	}
	if (viewRequired) {
		condition.insertAttribute(CONTAINER_ATTR_VIEW);
	}

	HandlerType handler(cxt);

	DataStoreV4 *dataStore = cxt.getSource().dataStore_;
	assert(dataStore != NULL);
	const DatabaseId dbId = cxt.getSource().dbId_;

	if (containerKey_ == NULL) {
		util::StackAllocator::Scope subScope(alloc);
		util::XArray< KeyDataStoreValue* > storeValueList(alloc);
		bool followingFound = dataStore->getKeyDataStore()->scanContainerList(
			txn, nextContainerId_,
			containerLimit_, dbId, condition, storeValueList);
		util::XArray< KeyDataStoreValue* >::iterator itr;
		for (itr = storeValueList.begin(); itr != storeValueList.end(); ++itr) {
			bool isAllowExpiration = true;
			ContainerAutoPtr containerAutoPtr(txn, dataStore, (*itr)->oId_, ANY_CONTAINER,
				isAllowExpiration);
			BaseContainer* baseContainer = containerAutoPtr.getBaseContainer();
			if (baseContainer == NULL) {
				GS_THROW_USER_ERROR(GS_ERROR_DS_CONTAINER_UNEXPECTEDLY_REMOVED, "");
			}
			handler(
				txn,
				(*itr)->containerId_,
				dbId,
				(*itr)->attribute_,
				*baseContainer);
		}
		if (!followingFound) {
			setNextContainerId(UNDEF_CONTAINERID);
		}
	}
	else {

		bool caseSensitive = false;
		bool allowExpiration = false;
		KeyDataStoreValue keyStoreValue = dataStore->getKeyDataStore()->get(txn, *containerKey_, caseSensitive);
		ContainerAutoPtr containerPtr(
			txn, dataStore,
			keyStoreValue.oId_, ANY_CONTAINER, allowExpiration);
		BaseContainer *container = containerPtr.getBaseContainer();
		do {
			if (container == NULL) {
				break;
			}

			const util::Set<ContainerAttribute> &attributeSet =
					condition.getAttributes();
			const ContainerAttribute attribute = container->getAttribute();
			if (attributeSet.find(attribute) == attributeSet.end()) {
				break;
			}

			handler(
					txn, container->getContainerId(), dbId, attribute,
					*container);
		}
		while (false);
		setNextContainerId(UNDEF_CONTAINERID);
	}
}

void MetaProcessor::setState(const MetaProcessor &src) {
	nextContainerId_ = src.nextContainerId_;
	containerLimit_ = src.containerLimit_;
	containerKey_ = src.containerKey_;
}


Value MetaProcessor::ValueUtils::makeNull() {
	Value dest;
	dest.setNull();
	return dest;
}

Value MetaProcessor::ValueUtils::makeString(
		util::StackAllocator &alloc, const char8_t *src) {
	char8_t *addr = const_cast<char8_t*>(src);
	const uint32_t size = static_cast<uint32_t>(strlen(src));

	Value dest;
	dest.set(alloc, addr, size);
	return dest;
}

Value MetaProcessor::ValueUtils::makeBool(bool src) {
	return Value(src);
}

Value MetaProcessor::ValueUtils::makeShort(int16_t src) {
	return Value(src);
}

Value MetaProcessor::ValueUtils::makeInteger(int32_t src) {
	return Value(src);
}

Value MetaProcessor::ValueUtils::makeLong(int64_t src) {
	return Value(src);
}

Value MetaProcessor::ValueUtils::makeDouble(double src) {
	return Value(src);
}

Value MetaProcessor::ValueUtils::makeTimestamp(Timestamp src) {
	Value value;
	value.setTimestamp(src);
	return value;
}

void MetaProcessor::ValueUtils::toUpperString(util::String &str) {
	for (util::String::iterator it = str.begin(); it != str.end(); ++it) {
		*it = Lexer::ToUpper(*it);
	}
}


MetaProcessor::ValueListSource::ValueListSource(
		ValueList &valueList, ValueCheckList &valueCheckList,
		const MetaContainerInfo &info) :
		valueList_(valueList),
		valueCheckList_(valueCheckList),
		info_(info) {
}


template<typename T>
MetaProcessor::ValueListBuilder<T>::ValueListBuilder(
		const ValueListSource &source) :
		source_(source) {
	assert(source_.valueList_.size() == source_.valueCheckList_.size());
	assert(source_.valueList_.size() == source_.info_.columnCount_);

	source_.valueCheckList_.assign(source_.valueCheckList_.size(), false);
}

template<typename T>
const MetaProcessor::ValueList& MetaProcessor::ValueListBuilder<T>::build() {
	if (std::find(
			source_.valueCheckList_.begin(),
			source_.valueCheckList_.end(), false) !=
			source_.valueCheckList_.end()) {
		assert(false);
		GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
	}

	return source_.valueList_;
}

template<typename T>
void MetaProcessor::ValueListBuilder<T>::set(T column, const Value &value) {
	assert(0 <= column);
	assert(static_cast<size_t>(column) < source_.info_.columnCount_);

	const MetaColumnInfo &info = source_.info_.columnList_[column];
	if (value.getType() != info.type_) {
		if (value.isNullValue()) {
			if (!info.nullable_) {
				assert(false);
				GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
			}
		}
		else {
			assert(false);
			GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
		}
	}

	source_.valueList_[column] = value;
	source_.valueCheckList_[column] = true;
}


MetaProcessor::Context::Context(
		TransactionContext &txn, MetaProcessor &processor,
		const Source &source, RowHandler &coreRowHandler) :
		processor_(processor),
		source_(source),
		valueList_(txn.getDefaultAllocator()),
		valueCheckList_(txn.getDefaultAllocator()),
		coreInfo_(processor.info_),
		valueListSource_(valueList_, valueCheckList_, coreInfo_),
		coreRowHandler_(coreRowHandler) {
	assert(processor.info_.forCore_);

	valueList_.assign(coreInfo_.columnCount_, ValueUtils::makeNull());
	valueCheckList_.assign(coreInfo_.columnCount_, false);
}

const MetaProcessorSource& MetaProcessor::Context::getSource() const {
	return source_;
}

const MetaProcessor::ValueListSource&
MetaProcessor::Context::getValueListSource() const {
	return valueListSource_;
}

MetaProcessor::RowHandler& MetaProcessor::Context::getRowHandler() const {
	return coreRowHandler_;
}

void MetaProcessor::Context::stepContainerListing(
		ContainerId lastContainerId) {
	assert(lastContainerId != UNDEF_CONTAINERID);
	processor_.nextContainerId_ = lastContainerId + 1;
}


int32_t MetaProcessor::SQLMetaUtils::toSQLColumnType(ColumnType type) {
	return COLUMN_TYPE_TABLE.get(type);
}

MetaProcessor::SQLMetaUtils::ColumnTypeTable::Entry
MetaProcessor::SQLMetaUtils::COLUMN_TYPE_TABLE_ENTRIES[] = {
	ColumnTypeTable::Entry(COLUMN_TYPE_BYTE, TYPE_TINYINT),
	ColumnTypeTable::Entry(COLUMN_TYPE_SHORT, TYPE_SMALLINT),
	ColumnTypeTable::Entry(COLUMN_TYPE_INT, TYPE_INTEGER),
	ColumnTypeTable::Entry(COLUMN_TYPE_LONG, TYPE_BIGINT),
	ColumnTypeTable::Entry(COLUMN_TYPE_FLOAT, TYPE_FLOAT),
	ColumnTypeTable::Entry(COLUMN_TYPE_DOUBLE, TYPE_DOUBLE),
	ColumnTypeTable::Entry(COLUMN_TYPE_TIMESTAMP, TYPE_TIMESTAMP),
	ColumnTypeTable::Entry(COLUMN_TYPE_BOOL, TYPE_BIT),
	ColumnTypeTable::Entry(COLUMN_TYPE_STRING, TYPE_VARCHAR),
	ColumnTypeTable::Entry(COLUMN_TYPE_BLOB, TYPE_BLOB)
};

const MetaProcessor::SQLMetaUtils::ColumnTypeTable
MetaProcessor::SQLMetaUtils::COLUMN_TYPE_TABLE(
		COLUMN_TYPE_TABLE_ENTRIES,
		sizeof(COLUMN_TYPE_TABLE_ENTRIES) /
				sizeof(*COLUMN_TYPE_TABLE_ENTRIES));

MetaProcessor::SQLMetaUtils::ColumnTypeTable::ColumnTypeTable(
		Entry *entryList, size_t count) :
		entryList_(entryList),
		count_(count) {
	std::sort(entryList, entryList + count);
}

int32_t MetaProcessor::SQLMetaUtils::ColumnTypeTable::get(
		ColumnType type) const {

	const Entry key(type, std::numeric_limits<int32_t>::min());
	const Entry *end = entryList_ + count_;

	const Entry *ret = std::upper_bound(entryList_, end, key);
	if (ret == end || ret->first != key.first) {
		return TYPE_OTHER;
	}

	return ret->second;
}


MetaProcessor::StoreCoreHandler::StoreCoreHandler(Context &cxt) :
		cxt_(cxt) {
}

void MetaProcessor::StoreCoreHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	if (attribute == CONTAINER_ATTR_SUB) {
		cxt_.stepContainerListing(id);
		return;
	}
	else if (attribute != CONTAINER_ATTR_LARGE) {
		execute(txn, id, dbId, attribute, container, NULL);
		cxt_.stepContainerListing(id);
		return;
	}

	BaseContainer *resolvedSubContainer;
	const FullContainerKey &key = container.getContainerKey(txn);
	const bool unnormalized = true;
	FullContainerKeyComponents components =
			key.getComponents(alloc, unnormalized);

	components.affinityString_ = NULL;
	components.affinityStringSize_ = 0;
	components.affinityNumber_ = txn.getPartitionId();
	components.largeContainerId_ = id;

	const FullContainerKey subKey(
		alloc, KeyConstraint::getNoLimitKeyConstraint(), components);

	const bool caseSensitive = true;
	KeyDataStoreValue keyStoreValue = container.getDataStore()->getKeyDataStore()->get(txn, subKey, caseSensitive);
	bool isAllowExpiration = false;
	ContainerAutoPtr subContainerPtr(
		txn, container.getDataStore(),
		keyStoreValue.oId_, ANY_CONTAINER, isAllowExpiration);
	resolvedSubContainer = subContainerPtr.getBaseContainer();
	if (resolvedSubContainer == NULL) {
		cxt_.stepContainerListing(id);
		return;
	}

	if (resolvedSubContainer->getAttribute() != CONTAINER_ATTR_SUB) {
		assert(false);
		GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
	}

	execute(txn, id, dbId, attribute, container, resolvedSubContainer);
	cxt_.stepContainerListing(id);
}

void MetaProcessor::StoreCoreHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(txn);
	static_cast<void>(id);
	static_cast<void>(dbId);
	static_cast<void>(attribute);
	static_cast<void>(container);
	static_cast<void>(subContainer);

	assert(false);
	GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
}

MetaProcessor::Context& MetaProcessor::StoreCoreHandler::getContext() const {
	return cxt_;
}

void MetaProcessor::StoreCoreHandler::getNames(
		TransactionContext &txn, BaseContainer &container,
		const char8_t *&dbName, const char8_t *&containerName) const {
	util::StackAllocator &alloc = txn.getDefaultAllocator();
	util::String *nameStr = ALLOC_NEW(alloc) util::String(alloc);
	container.getContainerKey(txn).toString(alloc, *nameStr);

	dbName = cxt_.getSource().dbName_;
	containerName = nameStr->c_str();
}


MetaProcessor::RefHandler::RefHandler(
		TransactionContext &txn, const MetaContainerInfo &refInfo,
		RowHandler &rowHandler) :
		refInfo_(refInfo),
		destValueList_(txn.getDefaultAllocator()),
		rowHandler_(rowHandler) {
	destValueList_.assign(refInfo_.columnCount_, ValueUtils::makeNull());
}

void MetaProcessor::RefHandler::operator()(
		TransactionContext &txn, const ValueList &valueList) {
	if (filter(txn, valueList)) {
		return;
	}

	for (size_t i = 0; i < refInfo_.columnCount_; i++) {
		const ColumnId refId = refInfo_.columnList_[i].refId_;
		assert(refId < valueList.size());

		destValueList_[i] = valueList[refId];
	}

	rowHandler_(txn, destValueList_);
}

bool MetaProcessor::RefHandler::filter(
		TransactionContext &txn, const ValueList &valueList) {
	static_cast<void>(txn);
	static_cast<void>(valueList);
	return false;
}


template<typename T>
void decodeLargeRow(
		const char *key, util::StackAllocator &alloc, TransactionContext &txn,
		DataStoreV4 *dataStore, const char8_t *dbName, BaseContainer *container,
		T &record, const EventMonotonicTime emNow);

const int8_t MetaProcessor::ContainerHandler::META_EXPIRATION_TYPE_ROW =
		EXPIRATION_TYPE_ROW;
const int8_t MetaProcessor::ContainerHandler::META_EXPIRATION_TYPE_PARTITION =
		EXPIRATION_TYPE_PARTITION;

MetaProcessor::ContainerHandler::ContainerHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::ContainerHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);

	ValueListBuilder<MetaType::ContainerMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	const ContainerType type = schemaContainer.getContainerType();

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	typedef TablePartitioningInfo<util::StackAllocator> PartitioningInfo;
	PartitioningInfo *partitioningInfo = NULL;
	if (attribute == CONTAINER_ATTR_LARGE) {
		util::StackAllocator &alloc = txn.getDefaultAllocator();
		partitioningInfo = ALLOC_NEW(alloc) PartitioningInfo(alloc);

		EventContext *eventContext =
				getContext().getSource().eventContext_;
		assert(eventContext != NULL);
		const int64_t emNow =
				eventContext->getHandlerStartMonotonicTime();
		try {
			decodeLargeRow(
				NoSQLUtils::LARGE_CONTAINER_KEY_PARTITIONING_INFO,
				alloc, txn, container.getDataStore(), dbName, &container,
				*partitioningInfo, emNow);
		}
		catch (std::exception& e) {
			ALLOC_DELETE(alloc, partitioningInfo);
			partitioningInfo = NULL;
			UTIL_TRACE_EXCEPTION(SQL_SERVICE, e, "");
		}
	}

	builder.set(
			MetaType::CONTAINER_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::CONTAINER_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::CONTAINER_ATTRIBUTE,
			ValueUtils::makeInteger(attribute));
	builder.set(
			MetaType::CONTAINER_TYPE_NAME,
			ValueUtils::makeString(alloc, containerTypeToName(type)));
	builder.set(
			MetaType::CONTAINER_NAME,
			ValueUtils::makeString(alloc, name));

	util::String dataAffinity(alloc);
	container.getAffinityStr(dataAffinity);
	builder.set(
			MetaType::CONTAINER_DATA_AFFINITY,
			(dataAffinity.length() == 0 ? ValueUtils::makeNull() :
			ValueUtils::makeString(alloc, dataAffinity.c_str())));

	const BaseContainer::ExpirationInfo *expirationInfo = NULL;
	int32_t expirationDivision = -1;

	if (partitioningInfo != NULL && partitioningInfo->isTableExpiration()) {
		BaseContainer::ExpirationInfo *partitionExpiration =
				ALLOC_NEW(alloc) BaseContainer::ExpirationInfo();
		partitionExpiration->elapsedTime_ =
				partitioningInfo->timeSeriesProperty_.elapsedTime_;
		partitionExpiration->timeUnit_ =
				partitioningInfo->timeSeriesProperty_.timeUnit_;
		expirationInfo = partitionExpiration;
	}

	builder.set(
			MetaType::CONTAINER_EXPIRATION_TIME,
			expirationInfo == NULL ? ValueUtils::makeNull() :
					ValueUtils::makeInteger(expirationInfo->elapsedTime_));
	builder.set(
			MetaType::CONTAINER_EXPIRATION_UNIT,
			expirationInfo == NULL ? ValueUtils::makeNull() : ValueUtils::makeString(
					alloc, timeUnitToName(expirationInfo->timeUnit_)));
	builder.set(
			MetaType::CONTAINER_EXPIRATION_DIVISION,
			ValueUtils::makeNull());
	builder.set(
			MetaType::CONTAINER_COMPRESSION_METHOD,
			ValueUtils::makeNull());

	builder.set(
			MetaType::CONTAINER_COMPRESSION_SIZE,
			ValueUtils::makeNull());
	builder.set(
			MetaType::CONTAINER_COMPRESSION_UNIT,
			ValueUtils::makeNull());

	for (size_t i = 0; i < 2; i++) {
		PartitioningInfo *p = partitioningInfo;
		if (p != NULL && i > 0 &&
				p->subPartitioningColumnId_ == UNDEF_COLUMNID) {
			p = NULL;
		}

		uint8_t partitionType = SyntaxTree::TABLE_PARTITION_TYPE_UNDEF;
		if (p != NULL) {
			partitionType = p->partitionType_;

			if (partitionType == SyntaxTree::TABLE_PARTITION_TYPE_RANGE_HASH) {
				partitionType = (i == 0 ?
						SyntaxTree::TABLE_PARTITION_TYPE_RANGE :
						SyntaxTree::TABLE_PARTITION_TYPE_HASH);
			}
		}

		const bool noInterval = (p == NULL ||
				partitionType != SyntaxTree::TABLE_PARTITION_TYPE_RANGE);
		const bool noTimestamp = (noInterval ||
				p->partitionColumnType_ != TupleList::TYPE_TIMESTAMP);

		builder.set(
				(i == 0 ?
						MetaType::CONTAINER_PARTITION_TYPE1 :
						MetaType::CONTAINER_PARTITION_TYPE2),
				p == NULL ? ValueUtils::makeNull() : ValueUtils::makeString(
						alloc, partitionTypeToName(partitionType)));
		builder.set(
				(i == 0 ?
						MetaType::CONTAINER_PARTITION_COLUMN1 :
						MetaType::CONTAINER_PARTITION_COLUMN2),
				p == NULL ? ValueUtils::makeNull() : ValueUtils::makeString(
						alloc, (i ==0 ?
								p->partitionColumnName_ :
								p->subPartitioningColumnName_).c_str()));
		builder.set(
				(i == 0 ?
						MetaType::CONTAINER_PARTITION_INTERVAL1 :
						MetaType::CONTAINER_PARTITION_INTERVAL2),
								(p == NULL || (p != NULL && !p->checkInterval(
										static_cast<int32_t>(i)))) ?
								ValueUtils::makeNull() : ValueUtils::makeString(
										alloc, p->getIntervalValue().c_str()));
		builder.set(
				(i == 0 ?
						MetaType::CONTAINER_PARTITION_UNIT1 :
						MetaType::CONTAINER_PARTITION_UNIT2),
				noTimestamp ? ValueUtils::makeNull() : ValueUtils::makeString(
						alloc, timeUnitToName(p->intervalUnit_)));

		builder.set(
				(i == 0 ?
						MetaType::CONTAINER_PARTITION_DIVISION1 :
						MetaType::CONTAINER_PARTITION_DIVISION2),
				(p == NULL || (p != NULL && !p->checkHashPartitioning(
						static_cast<int32_t>(i)))) ?
						ValueUtils::makeNull() :
						ValueUtils::makeInteger(p->getHashPartitioningCount(
								static_cast<int32_t>(i))));
	}

	builder.set(
			MetaType::CONTAINER_CLUSTER_PARTITION,
			ValueUtils::makeInteger(txn.getPartitionId()));

	{
		int8_t expirationType = -1;
		if (expirationInfo != NULL) {
			expirationType = META_EXPIRATION_TYPE_ROW;
		}
		PartitioningInfo *p = partitioningInfo;
		if (p != NULL && p->isTableExpiration()) {
			expirationType = META_EXPIRATION_TYPE_PARTITION;
		}
		builder.set(
				MetaType::CONTAINER_EXPIRATION_TYPE,
				expirationType < 0 ?
				ValueUtils::makeNull() : ValueUtils::makeString(
						alloc, expirationTypeToName(expirationType)));
	}

	getContext().getRowHandler()(txn, builder.build());
}

const char8_t* MetaProcessor::ContainerHandler::containerTypeToName(
		ContainerType type) {
	switch (type) {
	case TIME_SERIES_CONTAINER:
		return "TIMESERIES";
	case COLLECTION_CONTAINER:
		return "COLLECTION";
	default:
		assert(false);
		return "";
	}
}

const char8_t* MetaProcessor::ContainerHandler::timeUnitToName(
		TimeUnit unit) {
	switch (unit) {
	case TIME_UNIT_DAY:
		return "DAY";
	case TIME_UNIT_HOUR:
		return "HOUR";
	case TIME_UNIT_MINUTE:
		return "MINUTE";
	case TIME_UNIT_SECOND:
		return "SECOND";
	case TIME_UNIT_MILLISECOND:
		return "MILLISECOND";
	default:
		assert(false);
		return "";
	}
}

const char8_t* MetaProcessor::ContainerHandler::partitionTypeToName(
		uint8_t type) {
	switch (type) {
	case SyntaxTree::TABLE_PARTITION_TYPE_RANGE:
		return "INTERVAL";
	case SyntaxTree::TABLE_PARTITION_TYPE_HASH:
		return "HASH";
	default:
		assert(false);
		return "";
	}
}

const char8_t* MetaProcessor::ContainerHandler::expirationTypeToName(
		int8_t type) {
	switch (type) {
	case META_EXPIRATION_TYPE_ROW:
		return "ROW";
	case META_EXPIRATION_TYPE_PARTITION:
		return "PARTITION";
	default:
		assert(false);
		return "";
	}
}


MetaProcessor::ColumnHandler::ColumnHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::ColumnHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);

	ValueListBuilder<MetaType::ColumnMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	const ContainerType type = schemaContainer.getContainerType();

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	builder.set(
			MetaType::COLUMN_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::COLUMN_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::COLUMN_CONTAINER_ATTRIBUTE,
			ValueUtils::makeInteger(attribute));
	builder.set(
			MetaType::COLUMN_CONTAINER_NAME,
			ValueUtils::makeString(alloc, name));

	ObjectManagerV4 *objectManager =
			container.getDataStore()->getObjectManager();

	const uint32_t columnCount = schemaContainer.getColumnNum();
	int16_t keyCount = 0;
	for (uint32_t i = 0; i < columnCount; i++) {
		const ColumnInfo &info = schemaContainer.getColumnInfo(i);

		const ColumnType columnType = info.getColumnType();

		const bool nullable = !info.isNotNull();

		builder.set(
				MetaType::COLUMN_ORDINAL,
				ValueUtils::makeInteger(i + 1));

		const int32_t sqlColumnType =
				SQLMetaUtils::toSQLColumnType(columnType);
		builder.set(
				MetaType::COLUMN_SQL_TYPE,
				ValueUtils::makeInteger(sqlColumnType));

		const char8_t *columnTypeName =
				ValueProcessor::getTypeNameChars(columnType);

		builder.set(
				MetaType::COLUMN_TYPE_NAME,
				ValueUtils::makeString(alloc, columnTypeName));

		const char8_t *columnName =
				info.getColumnName(txn, *objectManager, const_cast<BaseContainer*>(&schemaContainer)->getMetaAllcateStrategy());
		builder.set(
				MetaType::COLUMN_NAME,
				ValueUtils::makeString(alloc, columnName));

		builder.set(
				MetaType::COLUMN_KEY,
				ValueUtils::makeBool(info.isKey()));
		builder.set(
				MetaType::COLUMN_NULLABLE,
				ValueUtils::makeBool(nullable));

		if (info.isKey()) {
			keyCount++;
		}
		builder.set(
				MetaType::COLUMN_KEY_SEQUENCE,
				ValueUtils::makeShort(info.isKey() ?
						keyCount : static_cast<int16_t>(-1)));

		Value compressionRelative = ValueUtils::makeNull();
		Value compressionRate = ValueUtils::makeNull();
		Value compressionSpan = ValueUtils::makeNull();
		Value compressionWidth = ValueUtils::makeNull();
		builder.set(
				MetaType::COLUMN_COMPRESSION_RELATIVE,
				compressionRelative);
		builder.set(
				MetaType::COLUMN_COMPRESSION_RATE,
				compressionRate);
		builder.set(
				MetaType::COLUMN_COMPRESSION_SPAN,
				compressionSpan);
		builder.set(
				MetaType::COLUMN_COMPRESSION_WIDTH,
				compressionWidth);

		getContext().getRowHandler()(txn, builder.build());
	}
}


MetaProcessor::IndexHandler::IndexHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::IndexHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);

	ValueListBuilder<MetaType::IndexMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	typedef util::Vector<IndexInfo> IndexInfoList;
	IndexInfoList indexInfoList(alloc);
	if (attribute == CONTAINER_ATTR_LARGE) {
		EventContext *eventContext =
				getContext().getSource().eventContext_;
		assert(eventContext != NULL);
		const int64_t emNow = eventContext->getHandlerStartMonotonicTime();

		TablePartitioningIndexInfo tablePartitioningIndexInfo(alloc);
		decodeLargeRow(
				NoSQLUtils::LARGE_CONTAINER_KEY_INDEX,
				alloc, txn, container.getDataStore(), dbName, &container,
				tablePartitioningIndexInfo, emNow);
		tablePartitioningIndexInfo.getIndexInfoList(alloc, indexInfoList);
	}
	else
	{
		container.getIndexInfoList(txn, indexInfoList);
	}

	builder.set(
			MetaType::INDEX_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::INDEX_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::INDEX_CONTAINER_NAME,
			ValueUtils::makeString(alloc, name));

	ObjectManagerV4 *objectManager =
			container.getDataStore()->getObjectManager();

	for (IndexInfoList::const_iterator it = indexInfoList.begin();
			it != indexInfoList.end(); ++it) {
		const IndexInfo &info = *it;

		const util::String &indexName = info.indexName_;
		const Value indexNameValue = indexName.empty() ?
				ValueUtils::makeNull() :
				ValueUtils::makeString(alloc, indexName.c_str());

		const int16_t indexType = static_cast<int16_t>(info.mapType);

		builder.set(
				MetaType::INDEX_NAME,
				indexNameValue);
		builder.set(
				MetaType::INDEX_TYPE,
				ValueUtils::makeShort(indexType));

		builder.set(
				MetaType::INDEX_TYPE_NAME,
				ValueUtils::makeString(alloc, getMapTypeStr(info.mapType)));

		const util::Vector<uint32_t> &columnList = info.columnIds_;
		assert(!columnList.empty());

		for (util::Vector<uint32_t>::const_iterator columnIt =
				columnList.begin(); columnIt != columnList.end(); ++columnIt) {
			const ColumnId columnId = *columnIt;
			const char8_t *columnName = schemaContainer.getColumnInfo(
					columnId).getColumnName(txn, *objectManager, container.getMetaAllcateStrategy());

			const int16_t ordinal =
					static_cast<int16_t>(columnIt - columnList.begin() + 1);

			builder.set(
					MetaType::INDEX_ORDINAL,
					ValueUtils::makeShort(ordinal));
			builder.set(
					MetaType::INDEX_COLUMN_NAME,
					ValueUtils::makeString(alloc, columnName));

			getContext().getRowHandler()(txn, builder.build());
		}
	}
}


MetaProcessor::MetaTriggerHandler::MetaTriggerHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::MetaTriggerHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);
	static_cast<void>(attribute);

	ValueListBuilder<MetaType::TriggerMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	typedef util::XArray<const uint8_t*> TriggerArray;
	TriggerArray triggerList(alloc);

	if (triggerList.empty()) {
		return;
	}

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	builder.set(
			MetaType::TRIGGER_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::TRIGGER_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::TRIGGER_CONTAINER_NAME,
			ValueUtils::makeString(alloc, name));

}


MetaProcessor::ErasableHandler::ErasableHandler(Context &cxt) :
		StoreCoreHandler(cxt), erasableTimeLimit_(MAX_TIMESTAMP) {
}

void MetaProcessor::ErasableHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {

	const ExpireType expireType = container.getExpireType();
	if (expireType != NO_EXPIRE) {
		execute(txn, id, dbId, attribute, container, NULL);
	}
	cxt_.stepContainerListing(id);
}

void MetaProcessor::ErasableHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);

	ValueListBuilder<MetaType::ErasableMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	const ContainerType type = schemaContainer.getContainerType();
	const ContainerId containerId = schemaContainer.getContainerId();
	const PartitionId pId = txn.getPartitionId();

	BaseContainer *archiveContainer = const_cast<BaseContainer *>(&schemaContainer);
	const FullContainerKey &cotainerKey = archiveContainer->getContainerKey(txn);
	FullContainerKeyComponents component = cotainerKey.getComponents(alloc);
	const ContainerId largeContainerId = component.largeContainerId_;
	uint32_t schemaVersionId = schemaContainer.getVersionId();
	int64_t initSchemaStatus = schemaContainer.getInitSchemaStatus();

	util::NormalOStringStream oss;
	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	ExpireType expireType = container.getExpireType();

	oss.str("");
	oss << dbId;
	builder.set(
			MetaType::ERASABLE_DATABASE_ID,
			ValueUtils::makeString(alloc, oss.str().c_str()));
	builder.set(
			MetaType::ERASABLE_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::ERASABLE_TYPE_NAME,
			ValueUtils::makeString(alloc, containerTypeToName(type)));
	oss.str("");
	oss << containerId;
	builder.set(
			MetaType::ERASABLE_CONTAINER_ID,
			ValueUtils::makeString(alloc, oss.str().c_str()));
	if (attribute != CONTAINER_ATTR_SUB) {
		builder.set(
				MetaType::ERASABLE_CONTAINER_NAME,
				ValueUtils::makeString(alloc, name));
		builder.set(
				MetaType::ERASABLE_PARTITION_NAME,
				ValueUtils::makeNull());
	} else {
		{
			FullContainerKeyComponents baseComponent = component;
			baseComponent.largeContainerId_ = UNDEF_LARGE_CONTAINERID;
			baseComponent.affinityNumber_ = UNDEF_NODE_AFFINITY_NUMBER;
			FullContainerKey baseCotainerKey(alloc,
				KeyConstraint::getNoLimitKeyConstraint(), baseComponent);

			util::String tableName(alloc);
			baseCotainerKey.toString(alloc, tableName);
			builder.set(
					MetaType::ERASABLE_CONTAINER_NAME,
					ValueUtils::makeString(alloc, tableName.c_str()));
		}
		{
			const int64_t affinity = static_cast<int64_t>(component.affinityNumber_);
			util::String partitionName(alloc);
			partitionName.append("#");
			partitionName.append(
				SQLProcessor::ValueUtils::toString(alloc, affinity));

			builder.set(
					MetaType::ERASABLE_PARTITION_NAME,
					ValueUtils::makeString(alloc, partitionName.c_str()));
		}
	}
	builder.set(
			MetaType::ERASABLE_CLUSTER_PARTITION,
			ValueUtils::makeInteger(pId));
	if (attribute != CONTAINER_ATTR_SUB) {
		builder.set(
				MetaType::ERASABLE_LARGE_CONTAINER_ID,
				ValueUtils::makeNull());
	} else {
		oss.str("");
		oss << largeContainerId;
		builder.set(
				MetaType::ERASABLE_LARGE_CONTAINER_ID,
				ValueUtils::makeString(alloc, oss.str().c_str()));
	}
	builder.set(
			MetaType::ERASABLE_SCHEMA_VERSION_ID,
			ValueUtils::makeInteger(schemaVersionId));
	oss.str("");
	oss << initSchemaStatus;
	builder.set(
			MetaType::ERASABLE_INIT_SCHEMA_STATUS,
			ValueUtils::makeString(alloc, oss.str().c_str()));
	builder.set(
			MetaType::ERASABLE_EXPIRATION_TYPE,
			ValueUtils::makeString(alloc, expirationTypeToName(expireType)));
	util::XArray< BaseContainer::ArchiveInfo > erasableList(alloc);
	container.getErasableList(txn, erasableTimeLimit_, erasableList);
	util::XArray< BaseContainer::ArchiveInfo >::iterator itr;
	for (itr = erasableList.begin(); itr != erasableList.end(); itr++) {
		builder.set(
				MetaType::ERASABLE_LOWER_BOUNDARY_TIME,
				ValueUtils::makeTimestamp(itr->start_));
		builder.set(
				MetaType::ERASABLE_UPPER_BOUNDARY_TIME,
				ValueUtils::makeTimestamp(itr->end_));
		builder.set(
				MetaType::ERASABLE_EXPIRATION_TIME,
				ValueUtils::makeTimestamp(itr->expired_));
		builder.set(
				MetaType::ERASABLE_ERASABLE_TIME,
				ValueUtils::makeTimestamp(itr->erasable_));
		oss.str("");
		oss << itr->rowIdMapOId_;
		builder.set(
				MetaType::ERASABLE_ROW_INDEX_OID,
				ValueUtils::makeString(alloc, oss.str().c_str()));
		oss.str("");
		oss << itr->mvccMapOId_;
		builder.set(
				MetaType::ERASABLE_MVCC_INDEX_OID,
				ValueUtils::makeString(alloc, oss.str().c_str()));

		getContext().getRowHandler()(txn, builder.build());
	}
}

const char8_t* MetaProcessor::ErasableHandler::containerTypeToName(
		ContainerType type) {
	return ContainerHandler::containerTypeToName(type);
}

const char8_t* MetaProcessor::ErasableHandler::expirationTypeToName(
		ExpireType type) {
	switch (type) {
	case TABLE_EXPIRE:
		return ContainerHandler::expirationTypeToName(
				ContainerHandler::META_EXPIRATION_TYPE_PARTITION);
	default:
		assert(false);
		return "";
	}
}


MetaProcessor::EventHandler::EventHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::EventHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	ValueListBuilder<MetaType::EventMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	PartitionTable *pt = getContext().getSource().partitionTable_;
	NodeAddress &address = pt->getNodeAddress(0, SYSTEM_SERVICE);

	EventMonitor &monitor = getContext().getSource().transactionManager_->getEventMonitor();
	util::StackAllocator::Scope scope(alloc);
	for (size_t pos = 0; pos < monitor.eventInfoList_.size(); pos++) {
		EventMonitor::EventInfo &eventInfo = monitor.eventInfoList_[pos];
		EventType eventType = eventInfo.eventType_;
		if (eventInfo.eventType_ != UNDEF_EVENT_TYPE) {
			builder.set(
					MetaType::EVENT_NODE_ADDRESS,
					ValueUtils::makeString(alloc, address.dump(false).c_str()));
			builder.set(
					MetaType::EVENT_NODE_PORT,
					ValueUtils::makeInteger(address.port_));

			builder.set(
					MetaType::EVENT_START_TIME,
					ValueUtils::makeTimestamp(
							eventInfo.startTime_));

			std::string applicationName;
			builder.set(
					MetaType::EVENT_APPLICATION_NAME,
					ValueUtils::makeNull());

			const char *serviceName;
			if (eventType < TXN_SHORTTERM_SYNC_REQUEST) {
				serviceName = "TRANSACTION";
			}
			else if (eventType < PARTITION_GROUP_START) {
				serviceName = "SYNC";
			}
			else if (eventType < SQL_NOTIFY_CLIENT) {
				serviceName = "CHECKPOINT";
			}
			else if (eventType < RECV_NOTIFY_MASTER) {
				serviceName = "SQL";
			}
			else {
				serviceName = "TRANSACTION_SERVICE";
			}

			builder.set(
					MetaType::EVENT_SERVICE_TYPE,
					ValueUtils::makeString(alloc, serviceName));

			builder.set(
					MetaType::EVENT_EVENT_TYPE,
					ValueUtils::makeString(alloc, getEventTypeName(eventType)));

			builder.set(
					MetaType::EVENT_WORKER_INDEX,
					ValueUtils::makeInteger(pos));

			builder.set(
					MetaType::EVENT_CLUSTER_PARTITION_INDEX,
					ValueUtils::makeInteger(eventInfo.pId_));

			getContext().getRowHandler()(txn, builder.build());
		}
	}
}


MetaProcessor::SocketHandler::SocketHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::SocketHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	static_cast<void>(id);
	static_cast<void>(dbId);
	static_cast<void>(attribute);
	static_cast<void>(container);

	ValueListBuilder<MetaType::SocketMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	if (getContext().getSource().transactionService_ == NULL) {
		assert(false);
		GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
	}

	if (getContext().getSource().sqlService_ == NULL) {
		assert(false);
		GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
	}

	EventEngine *eeList[] = {
		getContext().getSource().transactionService_->getEE(),
		getContext().getSource().sqlService_->getEE(),
		NULL
	};

	for (EventEngine **eeIt = eeList; *eeIt != NULL; ++eeIt) {
		typedef EventEngine::SocketInfo Info;
		typedef EventEngine::SocketStats Stats;
		typedef util::Vector< std::pair<Info, Stats> > StatsList;

		util::StackAllocator::Scope scope(alloc);

		EventEngine &ee = **eeIt;

		StatsList statsList(alloc);
		ee.getSocketStats(statsList);

		util::String eeName(ee.getName(), alloc);
		{
			const char8_t *suffix = "_SERVICE";
			const size_t pos = eeName.find(suffix);
			if (pos != util::String::npos &&
					pos + strlen(suffix) == eeName.size()) {
				eeName.erase(pos);
			}
		}

		for (StatsList::const_iterator it = statsList.begin();
				it != statsList.end(); ++it) {
			util::StackAllocator::Scope subScope(alloc);

			const Info &info = it->first;
			const Stats &stats = it->second;

			builder.set(
					MetaType::SOCKET_SERVICE_TYPE,
					ValueUtils::makeString(alloc, eeName.c_str()));

			const bool ndEmpty = info.nd_.isEmpty();
			const NodeDescriptor::Type ndType = info.multicast_ ?
					NodeDescriptor::ND_TYPE_MULTICAST :
					(ndEmpty ? NodeDescriptor::Type() : info.nd_.getType());
			const bool ndTypeSpecified = (!ndEmpty || info.multicast_);
			util::String ndTypeStr(
					(ndTypeSpecified ?
							NodeDescriptor::typeToString(ndType) : ""),
					alloc);
			ValueUtils::toUpperString(ndTypeStr);

			util::String applicationName(alloc);
			findApplicationName(info.nd_, applicationName);

			builder.set(
					MetaType::SOCKET_TYPE, ndTypeSpecified ?
							ValueUtils::makeString(alloc, ndTypeStr.c_str()) :
							ValueUtils::makeNull());

			buildSocketAddress(
					alloc, builder,
					MetaType::SOCKET_NODE_ADDRESS,
					MetaType::SOCKET_NODE_PORT, info.localAddress_);

			buildSocketAddress(
					alloc, builder,
					MetaType::SOCKET_REMOTE_ADDRESS,
					MetaType::SOCKET_REMOTE_PORT, info.remoteAddress_);

			builder.set(
					MetaType::SOCKET_APPLICATION_NAME,
					applicationName.empty() ?
							ValueUtils::makeNull() :
							ValueUtils::makeString(alloc, applicationName.c_str()));

			builder.set(
					MetaType::SOCKET_CREATION_TIME,
					ValueUtils::makeTimestamp(
							stats.initialTime_.getUnixTime()));

			builder.set(
					MetaType::SOCKET_DISPATCHING_EVENT_COUNT,
					ValueUtils::makeLong(stats.dispatchingEventCount_));
			builder.set(
					MetaType::SOCKET_SENDING_EVENT_COUNT,
					ValueUtils::makeLong(stats.sendingEventCount_));

			getContext().getRowHandler()(txn, builder.build());
		}
	}
}

bool MetaProcessor::SocketHandler::findApplicationName(
		const NodeDescriptor &nd, util::String &name) {
	name = "";

	if (nd.isEmpty() || nd.getType() != NodeDescriptor::ND_TYPE_CLIENT) {
		return false;
	}

	return StatementHandler::getApplicationNameByOptionsOrND(
			NULL, &nd, &name, NULL);
}

template<typename T>
void MetaProcessor::SocketHandler::buildSocketAddress(
		util::StackAllocator &alloc, ValueListBuilder<T> &builder,
		T addressType, T portType,
		const util::SocketAddress &socketAddress) const {
	if (socketAddress.isEmpty()) {
		builder.set(addressType, ValueUtils::makeNull());
		builder.set(portType, ValueUtils::makeNull());
	}
	else {
		u8string addressStr;
		uint16_t port;
		socketAddress.getIP(&addressStr, &port);

		builder.set(
				addressType,
				ValueUtils::makeString(alloc, addressStr.c_str()));
		builder.set(
				portType,
				ValueUtils::makeInteger(static_cast<int32_t>(port)));
	}
}

MetaProcessor::ContainerStatsHandler::ContainerStatsHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::ContainerStatsHandler::execute(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container,
		BaseContainer *subContainer) const {
	static_cast<void>(id);

	ValueListBuilder<MetaType::ContainerStatsMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const BaseContainer &schemaContainer =
			(subContainer == NULL ? container : *subContainer);

	const ContainerType type = schemaContainer.getContainerType();

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	builder.set(
			MetaType::CONTAINER_STATS_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::CONTAINER_STATS_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::CONTAINER_STATS_NAME,
			ValueUtils::makeString(alloc, name));
	OId groupId = container.getBaseGroupId();
	builder.set(
			MetaType::CONTAINER_STATS_GROUPID,
			ValueUtils::makeLong(groupId));
	builder.set(
			MetaType::CONTAINER_STATS_NUM_ROWS,
			attribute == CONTAINER_ATTR_LARGE ?
			ValueUtils::makeNull() : ValueUtils::makeLong(
				schemaContainer.getRowNum()));

	getContext().getRowHandler()(txn, builder.build());
}
const char8_t* MetaProcessor::ContainerStatsHandler::containerTypeToName(
		ContainerType type) {
	return ContainerHandler::containerTypeToName(type);
}


MetaProcessor::ClusterPartitionHandler::ClusterPartitionHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

const char *MetaProcessor::ClusterPartitionHandler::chunkCategoryList[] = {
	"META_DATA", "MAP_DATA", "ROW_DATA", "BATCH_FREE_MAP_DATA", "BATCH_FREE_ROW_DATA"
};

void MetaProcessor::ClusterPartitionHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {

	ValueListBuilder<MetaType::ClusterPartitionMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	PartitionTable *pt = getContext().getSource().partitionTable_;

	{
		uint32_t pId = txn.getPartitionId();
		LogSequentialNumber lsn = pt->getLSN(pId);
		builder.set(
				MetaType::CLUSTER_PARTITION_CLUSTER_PARTITION_INDEX,
				ValueUtils::makeInteger(pId));
		{
			std::string role;
			if (pt->isOwner(pId)) {
				role = "OWNER";
			}
			else if (pt->isBackup(pId)) {
				role = "BACKUP";
			}
			else if (pt->isCatchup(
						 pId, 0, PartitionTable::PT_CURRENT_OB)) {
				role = "CATCHUP";
			}
			else {
				role = "NONE";
			}
			builder.set(
					MetaType::CLUSTER_PARTITION_ROLE,
					ValueUtils::makeString(alloc, role.c_str()));
		}

		PartitionRole role;
		pt->getPartitionRole(pId, role);
		NodeAddress &address = pt->getNodeAddress(role.getOwner(), SYSTEM_SERVICE);
		builder.set(
			MetaType::CLUSTER_PARTITION_NODE_ADDRESS,
			ValueUtils::makeString(alloc, address.dump(false).c_str()));

		builder.set(
				MetaType::CLUSTER_PARTITION_NODE_PORT,
				ValueUtils::makeInteger(address.port_));

		builder.set(
				MetaType::CLUSTER_PARTITION_LSN,
				ValueUtils::makeLong(lsn));
		{
			std::string status =
				pt->dumpPartitionStatusForRest(pt->getPartitionStatus(pId));
			builder.set(
				MetaType::CLUSTER_PARTITION_STATUS,
				ValueUtils::makeString(alloc, status.c_str()));
		}

		ObjectManagerV4 *objectManager =
			container.getDataStore()->getObjectManager();
		objectManager->updateStoreObjectUseStats();

		Timestamp timestamp = getContext().getSource().eventContext_->getHandlerStartTime().getUnixTime();
		container.getDataStore()->scanChunkGroup(txn, timestamp);

		const uint32_t chunkSize = objectManager->getChunkSize();

		const DataStoreStats &dsStats = container.getDataStore()->stats();
		for (size_t chunkCategoryId = 0; chunkCategoryId < DS_CHUNK_CATEGORY_SIZE;
				++chunkCategoryId) {

			ChunkGroupStats::Table stats(NULL);
			dsStats.getChunkStats(chunkCategoryId, stats);

			builder.set(
					MetaType::CLUSTER_PARTITION_BLOCK_CATEGORY,
					ValueUtils::makeString(alloc, chunkCategoryList[chunkCategoryId]));

			const int64_t storeUse = static_cast<int64_t>(
					chunkSize *
					stats(ChunkGroupStats::GROUP_STAT_USE_BUFFER_COUNT).get());
			builder.set(
					MetaType::CLUSTER_PARTITION_STORE_USE,
					ValueUtils::makeLong(storeUse));

			const int64_t storeObjectUse = 0;
			builder.set(
					MetaType::CLUSTER_PARTITION_STORE_OBJECT_USE,
					ValueUtils::makeLong(storeObjectUse));

			getContext().getRowHandler()(txn, builder.build());
		}
	}
}



MetaProcessor::PartitionHandler::PartitionHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::PartitionHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	if (attribute != CONTAINER_ATTR_LARGE) {
		getContext().stepContainerListing(id);
		return;
	}

	ValueListBuilder<MetaType::PartitionMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	builder.set(
			MetaType::PARTITION_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::PARTITION_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::PARTITION_CONTAINER_NAME,
			ValueUtils::makeString(alloc, name));

	typedef TablePartitioningInfo<util::StackAllocator> PartitioningInfo;
	PartitioningInfo partitioningInfo(alloc);

	EventContext *eventContext =
			getContext().getSource().eventContext_;
	assert(eventContext != NULL);
	const int64_t emNow =
			eventContext->getHandlerStartMonotonicTime();

	try {
		decodeLargeRow(
			NoSQLUtils::LARGE_CONTAINER_KEY_PARTITIONING_INFO,
			alloc, txn, container.getDataStore(), dbName, &container,
			partitioningInfo, emNow);
	}
	catch (std::exception& e) {
		getContext().stepContainerListing(id);
		return;
	}

	TransactionManager *transactionManager =
			getContext().getSource().transactionManager_;
	assert(transactionManager != NULL);

	const int64_t clusterPartitionCount =
			transactionManager->getPartitionGroupConfig().getPartitionCount();
	if (clusterPartitionCount <= 0) {
		GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
	}
	const uint8_t partitionType = partitioningInfo.partitionType_;
	const bool forInterval = SyntaxTree::isRangePartitioningType(partitionType);
	const bool forTimestamp = (forInterval &&
			partitioningInfo.partitionColumnType_ == TupleList::TYPE_TIMESTAMP);

	typedef PartitioningInfo::PartitionAssignNumberList NumList;
	typedef PartitioningInfo::PartitionAssignStatusList StatusList;
	typedef PartitioningInfo::PartitionAssignValueList ValueList;

	const NumList &numList = partitioningInfo.assignNumberList_;
	const StatusList &statusList = partitioningInfo.assignStatusList_;
	const ValueList &valueList = partitioningInfo.assignValueList_;

	NumList::const_iterator numIt = numList.begin();
	StatusList::const_iterator statusIt = statusList.begin();
	ValueList::const_iterator valueIt = valueList.begin();

	const NumList::const_iterator numEnd = numList.end();
	const StatusList::const_iterator statusEnd = statusList.end();
	const ValueList::const_iterator valueEnd = valueList.end();

	const int64_t fixedAffinity = txn.getPartitionId();
	int64_t ordinal = 0;

	for (; numIt != numEnd && statusIt != statusEnd; ++numIt, ++statusIt) {
		const int64_t affinity = static_cast<int64_t>(*numIt);
		if (affinity == fixedAffinity) {
			if (valueIt != valueEnd) {
				++valueIt;
			}
			continue;
		}

		if ((*statusIt) != PARTITION_STATUS_CREATE_END) {
			if (valueIt != valueEnd) {
				valueIt++;
			}
			continue;
		}

		util::StackAllocator::Scope scope(alloc);

		builder.set(
				MetaType::PARTITION_ORDINAL,
				ValueUtils::makeLong(++ordinal));

		{
			util::String partitionName(alloc);
			partitionName.append("#");
			partitionName.append(
					SQLProcessor::ValueUtils::toString(alloc, affinity));
			builder.set(
					MetaType::PARTITION_NAME,
					ValueUtils::makeString(alloc, partitionName.c_str()));
		}

		Value boundary = ValueUtils::makeNull();
		if (forInterval && valueIt != valueEnd) {
			util::String boundaryStr(alloc);
			if (forTimestamp) {
				const int64_t &tsValue = *valueIt;
				boundaryStr = SQLProcessor::ValueUtils::toString(
						alloc, TupleValue(&tsValue, TupleList::TYPE_TIMESTAMP));
			}
			else {
				int64_t tmpValue = *valueIt;
				switch (partitioningInfo.partitionColumnType_) {
				case TupleList::TYPE_BYTE:
					if (tmpValue < std::numeric_limits<int8_t>::min()) {
						tmpValue = std::numeric_limits<int8_t>::min();
					}
					break;
				case TupleList::TYPE_SHORT:
					if (tmpValue < std::numeric_limits<int16_t>::min()) {
						tmpValue = std::numeric_limits<int16_t>::min();
					}
					break;
				case TupleList::TYPE_INTEGER:
					if (tmpValue < std::numeric_limits<int32_t>::min()) {
						tmpValue = std::numeric_limits<int32_t>::min();
					}
					break;
				}
				boundaryStr = SQLProcessor::ValueUtils::toString(
						alloc, TupleValue(tmpValue));
			}
			boundary = ValueUtils::makeString(alloc, boundaryStr.c_str());

			++valueIt;
		}

		builder.set(
				MetaType::PARTITION_BOUNDARY_VALUE1,
				boundary);
		builder.set(
				MetaType::PARTITION_BOUNDARY_VALUE2,
				ValueUtils::makeNull());
		builder.set(
				MetaType::PARTITION_NODE_AFFINITY,
				ValueUtils::makeLong(affinity));

		const int32_t clusterPartition =
				static_cast<int32_t>(affinity % clusterPartitionCount);
		builder.set(
				MetaType::PARTITION_CLUSTER_PARTITION_INDEX,
				ValueUtils::makeInteger(clusterPartition));

		PartitionTable *pt = getContext().getSource().partitionTable_;
		assert(pt != NULL);

		NodeId owner = pt->getNewSQLOwner(clusterPartition);
		if (owner != UNDEF_NODEID) {
			builder.set(
					MetaType::PARTITION_NODE_ADDRESS,
					ValueUtils::makeString(alloc, pt->dumpNodeAddress(
							owner, SQL_SERVICE).c_str()));
		}
		else {
			const char *nodeStr = "UNDEF";
			builder.set(
					MetaType::PARTITION_NODE_ADDRESS,
					ValueUtils::makeString(alloc, nodeStr));
		}
		builder.set(
				MetaType::PARTITION_STATUS,
				ValueUtils::makeString(alloc, getParitionStatusName(*statusIt)));

		getContext().getRowHandler()(txn, builder.build());
	}

	getContext().stepContainerListing(id);
}


MetaProcessor::ViewHandler::ViewHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::ViewHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	if (attribute != CONTAINER_ATTR_VIEW) {
		getContext().stepContainerListing(id);
		return;
	}

	ValueListBuilder<MetaType::ViewMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	const char8_t *dbName;
	const char8_t *name;
	getNames(txn, container, dbName, name);

	ViewInfo viewInfo(alloc);

	EventContext *eventContext =
			getContext().getSource().eventContext_;
	assert(eventContext != NULL);
	const int64_t emNow =
			eventContext->getHandlerStartMonotonicTime();
	try {
		decodeLargeRow(
				NoSQLUtils::LARGE_CONTAINER_KEY_VIEW_INFO,
				alloc, txn, container.getDataStore(), dbName, &container,
				viewInfo, emNow);
	} catch (std::exception &e) {
		getContext().stepContainerListing(id);
		return;
	}

	builder.set(
			MetaType::VIEW_DATABASE_ID,
			ValueUtils::makeLong(dbId));
	builder.set(
			MetaType::VIEW_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));
	builder.set(
			MetaType::VIEW_NAME,
			ValueUtils::makeString(alloc, name));

	builder.set(
			MetaType::VIEW_DEFINITION,
			ValueUtils::makeString(alloc, viewInfo.sqlString_.c_str()));

	getContext().getRowHandler()(txn, builder.build());

	getContext().stepContainerListing(id);
}


MetaProcessor::SQLHandler::SQLHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::SQLHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	ValueListBuilder<MetaType::SQLMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	PartitionTable *pt = getContext().getSource().partitionTable_;
	NodeAddress &address = pt->getNodeAddress(0, SYSTEM_SERVICE);
	SQLExecutionManager *executionManager
			= getContext().getSource().sqlExecutionManager_;
	if (executionManager == NULL) {
		return;
	}
	picojson::value jsonOutValue;
	util::Vector<ClientId> clientIdList(alloc);
	executionManager->getCurrentClientIdList(clientIdList);
	for (size_t pos = 0; pos < clientIdList.size(); pos++) {
		SQLExecutionManager::Latch latch(clientIdList[pos], executionManager);
		SQLExecution *execution = latch.get();
		if (execution) {
			const SQLExecution::SQLExecutionContext &sqlContext = execution->getContext();

			builder.set(
					MetaType::SQL_DATABASE_NAME,
					ValueUtils::makeString(alloc, sqlContext.getDBName()));

			builder.set(
				MetaType::SQL_NODE_ADDRESS,
				ValueUtils::makeString(alloc, address.dump(false).c_str()));

			builder.set(
					MetaType::SQL_NODE_PORT,
					ValueUtils::makeInteger(address.port_));

			builder.set(
					MetaType::SQL_START_TIME,
					ValueUtils::makeTimestamp(
							sqlContext.getStartTime()));

			builder.set(
					MetaType::SQL_APPLICATION_NAME,
					ValueUtils::makeString(alloc, sqlContext.getApplicationName()));

			builder.set(
					MetaType::SQL_SQL,
					ValueUtils::makeString(alloc, sqlContext.getQuery()));
			builder.set(
					MetaType::SQL_QUERY_ID,
					ValueUtils::makeString(alloc, sqlContext.getId().dump(alloc).c_str()));

			builder.set(
					MetaType::SQL_JOB_ID,
					ValueUtils::makeNull());
			getContext().getRowHandler()(txn, builder.build());
		}
	}
	JobManager *jobManager
			= getContext().getSource().sqlExecutionManager_->getJobManager();
	util::Vector<JobId> jobIdList(alloc);
	jobManager->getCurrentJobList(jobIdList);

	for (size_t pos = 0; pos < jobIdList.size(); pos++) {
		JobManager::Latch latch(jobIdList[pos], "MetaProcessor::SQLHandler::operator()", jobManager);
		Job *job = latch.get();
		if (job) {
			builder.set(
					MetaType::SQL_DATABASE_NAME,
					ValueUtils::makeNull());

			builder.set(
				MetaType::SQL_NODE_ADDRESS,
				ValueUtils::makeString(alloc, address.dump(false).c_str()));

			builder.set(
					MetaType::SQL_NODE_PORT,
					ValueUtils::makeInteger(address.port_));

			builder.set(
					MetaType::SQL_START_TIME,
					ValueUtils::makeTimestamp(
							job->getStartTime()));

			builder.set(
					MetaType::SQL_APPLICATION_NAME,
					ValueUtils::makeNull());

			builder.set(
					MetaType::SQL_SQL,
					ValueUtils::makeNull());

			builder.set(
					MetaType::SQL_QUERY_ID,
					ValueUtils::makeString(alloc, jobIdList[pos].dump(alloc, true).c_str()));
			builder.set(
					MetaType::SQL_JOB_ID,
					ValueUtils::makeString(alloc, jobIdList[pos].dump(alloc, false).c_str()));

			getContext().getRowHandler()(txn, builder.build());
		}
	}
}


MetaProcessor::PartitionStatsHandler::PartitionStatsHandler(Context &cxt) :
		StoreCoreHandler(cxt) {
}

void MetaProcessor::PartitionStatsHandler::operator()(
		TransactionContext &txn, ContainerId id, DatabaseId dbId,
		ContainerAttribute attribute, BaseContainer &container) const {
	if (attribute != CONTAINER_ATTR_SUB) {
		getContext().stepContainerListing(id);
		return;
	}

	ValueListBuilder<MetaType::PartitionStatsMeta> builder(
			getContext().getValueListSource());
	util::StackAllocator &alloc = txn.getDefaultAllocator();


	builder.set(
			MetaType::PARTITION_STATS_DATABASE_ID,
			ValueUtils::makeLong(dbId));

	const char8_t *dbName = cxt_.getSource().dbName_;
	builder.set(
			MetaType::PARTITION_STATS_DATABASE_NAME,
			ValueUtils::makeString(alloc, dbName));

	FullContainerKey containerKey = container.getContainerKey(txn);
	FullContainerKeyComponents component = containerKey.getComponents(alloc);

	{
		util::String containerName(alloc);
		containerName.append(component.baseName_, component.baseNameSize_);
		builder.set(
				MetaType::PARTITION_STATS_CONTAINER_NAME,
				ValueUtils::makeString(alloc, containerName.c_str()));
	}

	{
		const int64_t affinity = static_cast<int64_t>(component.affinityNumber_);
		util::String partitionName(alloc);
		partitionName.append("#");
		partitionName.append(
			SQLProcessor::ValueUtils::toString(alloc, affinity));

		builder.set(
				MetaType::PARTITION_STATS_NAME,
				ValueUtils::makeString(alloc, partitionName.c_str()));
	}

	builder.set(
			MetaType::PARTITION_STATS_NUM_ROWS,
			ValueUtils::makeLong(container.getRowNum()));
	getContext().getRowHandler()(txn, builder.build());

	getContext().stepContainerListing(id);
}





MetaProcessor::KeyRefHandler::KeyRefHandler(
		TransactionContext &txn, const MetaContainerInfo &refInfo,
		RowHandler &rowHandler) :
		RefHandler(txn, refInfo, rowHandler) {
	assert(refInfo.id_ == MetaType::TYPE_KEY);
}

bool MetaProcessor::KeyRefHandler::filter(
		TransactionContext &txn, const ValueList &valueList) {
	static_cast<void>(txn);

	assert(MetaType::COLUMN_KEY < valueList.size());
	const Value &keyValue = valueList[MetaType::COLUMN_KEY];

	assert(keyValue.getType() == COLUMN_TYPE_BOOL);
	return !keyValue.getBool();
}


MetaProcessor::ContainerRefHandler::ContainerRefHandler(
		TransactionContext &txn, const MetaContainerInfo &refInfo,
		RowHandler &rowHandler) :
		RefHandler(txn, refInfo, rowHandler) {
	assert(refInfo.id_ == MetaType::TYPE_CONTAINER);
}

bool MetaProcessor::ContainerRefHandler::filter(
		TransactionContext &txn, const ValueList &valueList) {
	static_cast<void>(txn);

	assert(MetaType::CONTAINER_ATTRIBUTE < valueList.size());
	const Value &attributeValue = valueList[MetaType::CONTAINER_ATTRIBUTE];

	assert(attributeValue.getType() == COLUMN_TYPE_INT);
	return (attributeValue.getInt() == CONTAINER_ATTR_VIEW);
}


MetaProcessorSource::MetaProcessorSource(
		DatabaseId dbId, const char8_t *dbName) :
		dbId_(dbId),
		dbName_(dbName),
		eventContext_(NULL),
		transactionManager_(NULL),
		transactionService_(NULL),
		partitionTable_(NULL),
		sqlExecutionManager_(NULL),
		sqlService_(NULL)
{
}


MetaContainer::MetaContainer(
		TransactionContext &txn, DataStoreV4 *dataStore, DatabaseId dbId,
		MetaContainerId id, NamingType containerNamingType,
		NamingType columnNamingType) :
		BaseContainer(txn, dataStore),
		info_(MetaType::InfoTable::getInstance().resolveInfo(id, false)),
		containerNamingType_(containerNamingType),
		columnNamingType_(columnNamingType),
		dbId_(dbId),
		columnInfoList_(NULL) {
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	baseContainerImage_ = ALLOC_NEW(alloc) BaseContainerImage;
	memset(baseContainerImage_, 0, sizeof(BaseContainerImage));
	baseContainerImage_->containerId_ = UNDEF_CONTAINERID;
	baseContainerImage_->versionId_ = info_.versionId_;
	baseContainerImage_->containerType_ = COLLECTION_CONTAINER;

	columnInfoList_ = ALLOC_NEW(alloc) util::XArray<ColumnInfo>(alloc);
}

MetaContainerId MetaContainer::getMetaContainerId() const {
	return info_.id_;
}

MetaType::NamingType MetaContainer::getContainerNamingType() const {
	return containerNamingType_;
}

MetaType::NamingType MetaContainer::getColumnNamingType() const {
	return columnNamingType_;
}

FullContainerKey MetaContainer::getContainerKey(TransactionContext &txn) {
	const char8_t *name =
			MetaType::InfoTable::nameOf(info_, containerNamingType_, false);
	FullContainerKeyComponents components;
	components.dbId_ = dbId_;
	components.systemPart_ = name;
	components.systemPartSize_ = strlen(name);
	return FullContainerKey(
			txn.getDefaultAllocator(),
			KeyConstraint::getNoLimitKeyConstraint(), components);
}

const MetaContainerInfo& MetaContainer::getMetaContainerInfo() const {
	return info_;
}

void MetaContainer::getContainerInfo(
		TransactionContext &txn,
		util::XArray<uint8_t> &containerSchema, bool optionIncluded, bool internalOptionIncluded) {

	uint32_t columnNum = getColumnNum();
	containerSchema.push_back(
			reinterpret_cast<uint8_t*>(&columnNum), sizeof(uint32_t));

	for (uint32_t i = 0; i < columnNum; i++) {
		getColumnSchema(
				txn, i, *getObjectManager(), containerSchema);
	}

	{
		util::Vector<ColumnId> keyColumnIdList(txn.getDefaultAllocator());
		getKeyColumnIdList(keyColumnIdList);
		int16_t rowKeyNum = static_cast<int16_t>(keyColumnIdList.size());
		containerSchema.push_back(
				reinterpret_cast<uint8_t *>(&rowKeyNum), sizeof(int16_t));
		util::Vector<ColumnId>::iterator itr;
		for (itr = keyColumnIdList.begin(); itr != keyColumnIdList.end(); itr++) {
			int16_t rowKeyColumnId = static_cast<int16_t>(*itr);
			containerSchema.push_back(
				reinterpret_cast<uint8_t *>(&rowKeyColumnId), sizeof(int16_t));
		}
	}

	if (optionIncluded) {
		getCommonContainerOptionInfo(containerSchema);
		getContainerOptionInfo(txn, containerSchema);
		if (internalOptionIncluded) {
			int32_t containerAttribute = getAttribute();
			containerSchema.push_back(
				reinterpret_cast<uint8_t *>(&containerAttribute), sizeof(int32_t));
			TablePartitioningVersionId tablePartitioningVersionId = -1;
			containerSchema.push_back(
				reinterpret_cast<const uint8_t *>(&tablePartitioningVersionId),
				sizeof(TablePartitioningVersionId));
			int32_t elapsedTime = -1;
			TimeUnit timeUnit = UINT8_MAX;
			int64_t startValue = -1;
			int64_t limitValue = -1;

			containerSchema.push_back(
				reinterpret_cast<const uint8_t *>(&(elapsedTime)),
				sizeof(int32_t));
			int8_t tmp = static_cast<int8_t>(timeUnit);
			containerSchema.push_back(
				reinterpret_cast<const uint8_t *>(&tmp), sizeof(int8_t));
			containerSchema.push_back(
				reinterpret_cast<const uint8_t *>(&startValue),
				sizeof(Timestamp));
			containerSchema.push_back(
				reinterpret_cast<const uint8_t *>(&(limitValue)),
				sizeof(int64_t));
		}
	}
}

void MetaContainer::getIndexInfoList(
		TransactionContext &txn, util::Vector<IndexInfo> &indexInfoList) {
	static_cast<void>(txn);
	static_cast<void>(indexInfoList);
}

uint32_t MetaContainer::getColumnNum() const {
	return static_cast<uint32_t>(info_.columnCount_);
}

void MetaContainer::getKeyColumnIdList(
		util::Vector<ColumnId> &keyColumnIdList) {
	static_cast<void>(keyColumnIdList);
}

void MetaContainer::getCommonContainerOptionInfo(
		util::XArray<uint8_t> &containerSchema) {

	const char8_t *affinityStr = "";
	int32_t affinityStrLen = static_cast<int32_t>(strlen(affinityStr));
	containerSchema.push_back(
			reinterpret_cast<uint8_t*>(&affinityStrLen), sizeof(int32_t));
	containerSchema.push_back(
			reinterpret_cast<const uint8_t*>(affinityStr), affinityStrLen);
}

void MetaContainer::getColumnSchema(
		TransactionContext &txn, uint32_t columnId,
		ObjectManagerV4 &objectManager, util::XArray<uint8_t> &schema) {

	const char8_t *columnName = const_cast<char *>(
			getColumnName(txn, columnId, objectManager));
	int32_t columnNameLen = static_cast<int32_t>(strlen(columnName));
	schema.push_back(
			reinterpret_cast<uint8_t*>(&columnNameLen), sizeof(int32_t));
	schema.push_back(
			reinterpret_cast<const uint8_t*>(columnName), columnNameLen);

	int8_t tmp = static_cast<int8_t>(getSimpleColumnType(columnId));
	schema.push_back(reinterpret_cast<uint8_t*>(&tmp), sizeof(int8_t));

	uint8_t flag = (isArrayColumn(columnId) ? 1 : 0);
	flag |= (isVirtualColumn(columnId) ? ColumnInfo::COLUMN_FLAG_VIRTUAL : 0);
	flag |= (isNotNullColumn(columnId) ? ColumnInfo::COLUMN_FLAG_NOT_NULL : 0);
	schema.push_back(reinterpret_cast<uint8_t*>(&flag), sizeof(uint8_t));
}

const char8_t* MetaContainer::getColumnName(
		TransactionContext &txn, uint32_t columnId,
		ObjectManagerV4 &objectManager) const {
	static_cast<void>(txn);
	static_cast<void>(objectManager);
	assert(columnId < info_.columnCount_);
	return MetaType::InfoTable::nameOf(
			info_.columnList_[columnId], columnNamingType_, false);
}

ColumnType MetaContainer::getSimpleColumnType(uint32_t columnId) const {
	assert(columnId < info_.columnCount_);
	return info_.columnList_[columnId].type_;
}

bool MetaContainer::isArrayColumn(uint32_t columnId) const {
	static_cast<void>(columnId);
	return false;
}

bool MetaContainer::isVirtualColumn(uint32_t columnId) const {
	static_cast<void>(columnId);
	return false;
}

bool MetaContainer::isNotNullColumn(uint32_t columnId) const {
	assert(columnId < info_.columnCount_);
	return !info_.columnList_[columnId].nullable_;
}

void MetaContainer::getTriggerList(
		TransactionContext &txn, util::XArray<const uint8_t*> &triggerList) {
	static_cast<void>(txn);
	static_cast<void>(triggerList);
}

ContainerAttribute MetaContainer::getAttribute() const {
	return CONTAINER_ATTR_SINGLE;
}

void MetaContainer::getNullsStats(util::XArray<uint8_t> &nullsList) const {
	static_cast<void>(nullsList);
	GS_THROW_USER_ERROR(GS_ERROR_CM_INTERNAL_ERROR, "");
}

void MetaContainer::getColumnInfoList(
		util::XArray<ColumnInfo> &columnInfoList) const {

	uint32_t columnCount = getColumnNum();
	uint16_t variableColumnIndex = 0;
	uint32_t nullsAndVarOffset =
			ValueProcessor::calcNullsByteSize(columnCount);
	uint32_t rowFixedSize = 0;
	for (uint32_t i = 0; i < columnCount; i++) {
		ColumnInfo info;
		info.initialize();
		info.setType(getSimpleColumnType(i), false);
		if (info.isVariable()) {
			info.setOffset(variableColumnIndex);
			variableColumnIndex++;
		}
		else {
			rowFixedSize += info.getColumnSize();
		}
		columnInfoList.push_back(info);
	}
	if (variableColumnIndex > 0) {
		nullsAndVarOffset += sizeof(OId);
		rowFixedSize = 0;
		for (uint32_t i = 0; i < columnCount; i++) {
			ColumnInfo &info = columnInfoList[i];
			if (!info.isVariable()) {
				info.setOffset(nullsAndVarOffset + rowFixedSize);
				rowFixedSize += info.getColumnSize();
			}
		}
	}
}

void MetaContainer::getColumnInfo(
		TransactionContext &txn, ObjectManagerV4 &objectManager,
		const char8_t *name, uint32_t &columnId, ColumnInfo *&columnInfo,
		bool isCaseSensitive) const {

	columnId = UNDEF_COLUMNID;
	columnInfo = NULL;

	const uint32_t columnNum = getColumnNum();
	for (uint32_t i = 0; i < columnNum; i++) {
		const char8_t *columnName = getColumnName(txn, i, objectManager);
		uint32_t columnNameSize = static_cast<uint32_t>(strlen(columnName));
		bool isExist = eqCaseStringString(
				txn, name, static_cast<uint32_t>(strlen(name)),
				columnName, columnNameSize, isCaseSensitive);
		if (isExist) {
			columnId = i;
			columnInfo = &getColumnInfo( i);
			return;
		}
	}
}

ColumnInfo& MetaContainer::getColumnInfo(uint32_t columnId) const {
	if (columnInfoList_->empty()) {
		util::XArray<ColumnInfo> columnInfoList(
				*columnInfoList_->get_allocator().base());
		getColumnInfoList(columnInfoList);
		columnInfoList_->swap(columnInfoList);
	}

	return (*columnInfoList_)[columnId];
}

void MetaContainer::getContainerOptionInfo(
		TransactionContext &txn, util::XArray<uint8_t> &containerSchema) {
	static_cast<void>(txn);
	static_cast<void>(containerSchema);
}


MetaStore::MetaStore(DataStoreV4 &dataStore) : dataStore_(dataStore) {
}

MetaContainer* MetaStore::getContainer(
		TransactionContext &txn, const FullContainerKey &key,
		MetaType::NamingType defaultNamingType) {
	util::StackAllocator &alloc = txn.getDefaultAllocator();

	FullContainerKeyComponents components =
			key.getComponents(alloc);
	if (components.systemPartSize_ == 0) {
		return NULL;
	}

	const DatabaseId dbId = components.dbId_;
	const util::String systemName(
			components.systemPart_, components.systemPartSize_,
			alloc);

	components = FullContainerKeyComponents();
	components.dbId_ = dbId;
	components.systemPart_ = systemName.c_str();
	components.systemPartSize_ = systemName.size();
	const FullContainerKey systemKey(
			alloc, KeyConstraint::getNoLimitKeyConstraint(), components);

	if (key.compareTo(alloc, systemKey, true) != 0) {
		return NULL;
	}

	const MetaType::InfoTable &infoTable = MetaType::InfoTable::getInstance();
	MetaType::NamingType namingType;
	const MetaContainerInfo *info =
			infoTable.findInfo(systemName.c_str(), false, namingType);
	if (info == NULL) {
		return NULL;
	}

	const MetaType::NamingType columnNamingType =
			MetaType::InfoTable::resolveNaming(namingType, defaultNamingType);
	return ALLOC_NEW(alloc) MetaContainer(
			txn, &dataStore_, dbId, info->id_, namingType, columnNamingType);
}

MetaContainer* MetaStore::getContainer(
		TransactionContext &txn, DatabaseId dbId, MetaContainerId id,
		MetaType::NamingType containerNamingType,
		MetaType::NamingType columnNamingType) {
	util::StackAllocator &alloc = txn.getDefaultAllocator();
	return ALLOC_NEW(alloc) MetaContainer(
			txn, &dataStore_, dbId, id, containerNamingType, columnNamingType);
}
