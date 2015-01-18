#include <DB/IO/ReadHelpers.h>
#include <DB/IO/WriteHelpers.h>

#include <DB/Core/Block.h>

#include <DB/DataStreams/BlockStreamProfileInfo.h>


namespace DB
{

void BlockStreamProfileInfo::read(ReadBuffer & in)
{
	readVarUInt(rows, in);
	readVarUInt(blocks, in);
	readVarUInt(bytes, in);
	readBinary(applied_limit, in);
	readVarUInt(rows_before_limit, in);
	readBinary(calculated_rows_before_limit, in);
}


void BlockStreamProfileInfo::write(WriteBuffer & out) const
{
	writeVarUInt(rows, out);
	writeVarUInt(blocks, out);
	writeVarUInt(bytes, out);
	writeBinary(hasAppliedLimit(), out);
	writeVarUInt(getRowsBeforeLimit(), out);
	writeBinary(calculated_rows_before_limit, out);
}


size_t BlockStreamProfileInfo::getRowsBeforeLimit() const
{
	if (!calculated_rows_before_limit)
		calculateRowsBeforeLimit();
	return rows_before_limit;
}


bool BlockStreamProfileInfo::hasAppliedLimit() const
{
	if (!calculated_rows_before_limit)
		calculateRowsBeforeLimit();
	return applied_limit;
}


void BlockStreamProfileInfo::update(Block & block)
{
	++blocks;
	rows += block.rowsInFirstColumn();
	bytes += block.bytes();

	if (column_names.empty())
		column_names = block.dumpNames();
}


void BlockStreamProfileInfo::collectInfosForStreamsWithName(const char * name, BlockStreamProfileInfos & res) const
{
	if (stream_name == name)
	{
		res.push_back(this);
		return;
	}

	for (const auto & nested_info : nested_infos)
		nested_info->collectInfosForStreamsWithName(name, res);
}


void BlockStreamProfileInfo::calculateRowsBeforeLimit() const
{
	calculated_rows_before_limit = true;

	/// есть ли Limit?
	BlockStreamProfileInfos limits;
	collectInfosForStreamsWithName("Limit", limits);
	if (limits.empty())
		return;

	applied_limit = true;

	/** Берём количество строчек, прочитанных ниже PartialSorting-а, если есть, или ниже Limit-а.
	  * Это нужно, потому что сортировка может вернуть только часть строк.
	  */
	BlockStreamProfileInfos partial_sortings;
	collectInfosForStreamsWithName("PartialSorting", partial_sortings);

	BlockStreamProfileInfos & limits_or_sortings = partial_sortings.empty() ? limits : partial_sortings;

	for (const auto & info_limit_or_sort : limits_or_sortings)
		for (const auto & nested_info : info_limit_or_sort->nested_infos)
			rows_before_limit += nested_info->rows;
}

}