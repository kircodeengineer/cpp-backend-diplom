#include "postgres.h"
#include <pqxx/zview.hxx>
#include <iostream>
#include "random_functions.h"

namespace postgres
{

	using namespace std::literals;
	using pqxx::operator"" _zv;

	void CreateTable(pqxx::connection& connection)
	{
		pqxx::work work{ connection };
		work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
id UUID CONSTRAINT firstindex PRIMARY KEY,
name varchar(100) NOT NULL,
score int,
playTime int
);
)"_zv);

		work.exec(R"(
CREATE INDEX IF NOT EXISTS scores_rating ON
retired_players (score DESC, playtime, name)
;
)"_zv);

		work.commit();
	}


	void WriteRetiredToDatabase(pqxx::connection& conn, const std::vector<RetiredPlayer >& vec_input)
	{
		pqxx::work work{ conn };

		for (auto iter = vec_input.begin(); iter != vec_input.end(); iter++)
		{
			work.exec_params(R"(INSERT INTO 
retired_players (id, name, score, playtime) 
VALUES ($1, $2, $3, $4)
)"_zv,
random_functions::RandomHexString(32), iter->name, iter->score, iter->play_time_s);
		}
		work.commit();
	}


	void ReadRetiredFromDatabase(pqxx::connection& conn, int start, int max_item, std::vector < RetiredPlayer >& vec_input)
	{
		pqxx::read_transaction read_trans(conn);
		auto query_text = "SELECT name, score, playtime FROM retired_players ORDER BY score DESC, playtime, name OFFSET " +
			read_trans.quote(start) + " LIMIT " + read_trans.quote(max_item) +
			";";
		for (auto [name, score, playtime] : read_trans.query<std::string, int, int>(query_text)) {

			vec_input.push_back({ 0, name, score, playtime });
		}
	}
}
