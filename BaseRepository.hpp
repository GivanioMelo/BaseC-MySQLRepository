#include <iostream>
#include <string>
#include <ctime>   // Para time_t, time, localtime, strftime
#include <iomanip> // Para std::put_time
#include <vector>
#include <stdexcept>
#include <memory>   // Para std::unique_ptr
#include <fstream>  // Para std::ifstream (leitura de arquivo)
#include <sstream>  // Para std::stringstream
#include <map>      // Para std::map (armazenar configura��es)

// Inclua os cabe�alhos do MySQL Connector/C++
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

// Definindo aliases
using Row = std::vector<std::string>;
using DataSet = std::vector<Row>;
using Dictionary = std::map<std::string, std::string>;

#ifndef ENTITY
#define ENTITY

// Classe base para todas as entidades no sistema
class Entity {
protected:
    long long id;
    std::string createdAt;
    std::string updatedAt;
    std::string tableName = "entities";
public:
    Entity()
    {
        id = 0;
        time_t currentTime = std::time(nullptr);
        createdAt = ctime(&currentTime);
        updatedAt = ctime(&currentTime);
    }

    virtual ~Entity() {
        std::cout << "Entidade destru�da" << std::endl;
    }

    long long getId() const {
        return id;
    }

    std::string getCreatedAt() const {
        return createdAt;
    }

    // Retorna a data e hora da �ltima atualiza��o formatada
    std::string getUpdatedAt() const {
        return updatedAt;
    }

    // --- M�todos Setters para o Reposit�rio ---
    void setId(long long newId) { id = newId; }
    void setCreatedAt(std::time_t time) { createdAt = ctime(&time); }
    void setUpdatedAt(std::time_t time) { updatedAt = ctime(&time); }

    void updateTimestamp() {
        time_t currentTime = std::time(nullptr);
        updatedAt = ctime(&currentTime);
    }

    // M�todo virtual puro para exibir informa��es da entidade (deve ser implementado por classes derivadas)
    virtual void displayInfo() const = 0;


    // Exemplo de sobrecarga do operador de igualdade para comparar entidades por ID
    bool operator==(const Entity& other) const {
        return this->id == other.id;
    }

    // Exemplo de sobrecarga do operador de diferente
    bool operator!=(const Entity& other) { return !(*this == other); }

    // M�todos virtuais puros para o mapeamento do reposit�rio
   // Devem retornar o nome da tabela no DB
    virtual std::string getTableName() const = 0;
    // Devem retornar uma lista ordenada dos nomes das colunas no DB, excluindo o ID (que � autoincrementado)
    // Usado para INSERTs
    virtual std::vector<std::string> getColumnNamesForInsert() const = 0;
    // Devem retornar uma lista ordenada dos valores dos campos da entidade para INSERT
    virtual std::vector<std::string> getValuesForInsert() const = 0;
    // Devem retornar uma lista ordenada de pares "coluna=valor" para UPDATE
    virtual std::vector<std::string> getUpdatePairs() const = 0;
    // Devem retornar uma lista ordenada dos nomes das colunas no DB, incluindo o ID
    // Usado para SELECTs
    virtual std::vector<std::string> getColumnNamesForSelect() const = 0;
    // Deve preencher o objeto a partir de uma linha de ResultSetData
    virtual void fillFromRow(const Row& row) = 0;
};

#endif // !ENTITY_HPP

#ifndef BASE_REPOSITORY
#define BASE_REPOSITORY

const std::string CONFIG_FILE = "db_config.ini";

Dictionary loadConfigurations()
{
    std::ifstream file(CONFIG_FILE);
    if (!file.is_open())
    {
        std::cerr << "Erro: N�o foi poss�vel abrir o arquivo de configura��o: " << CONFIG_FILE << std::endl;
        return Dictionary();
    }

    std::string line;
    Dictionary configurations;

    while (std::getline(file, line)) {
        // Ignorar linhas vazias ou coment�rios
        if (line.empty() || line[0] == '#') {
            continue;
        }
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            // Remover espa�os em branco extras (trim)
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            configurations[key] = value;
        }
    }
    file.close();
    return configurations;
}

// Classe auxiliar para armazenar e carregar configura��es do banco de dados
class DatabaseConfig {
public:
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    std::string port;

    /**
     * @brief Carrega as configura��es do banco de dados de um arquivo.
     * O formato esperado � chave=valor por linha.
     * @param filename O nome do arquivo de configura��o.
     * @return true se as configura��es foram carregadas com sucesso, false caso contr�rio.
     */
    bool loadFromFile() {
        
        Dictionary config = loadConfigurations();
        if (config.size() == 0) return false;
        // Atribuir valores do mapa aos membros da classe
        host = config["host"];
        user = config["user"];
        password = config["password"];
        database = config["database"];
        
        port = config.count("port") ? config["port"] : "3306"; // Padr�o 3306 se n�o especificado

        // Verificar se todos os campos obrigat�rios sem valor padr�o foram carregados
        if (host.empty())return false;
        if (user.empty())return false;
        if (password.empty())return false;
        if (database.empty())return false;
        return true;
    }
};

using Connection = std::unique_ptr<sql::Connection>;

// BaseRepository agora � uma classe template
template<typename T> // T deve herdar de Entity
class BaseRepository {
private:
    sql::Driver* driver;

    // As credenciais agora s�o membros da classe, carregadas de DatabaseConfig
    const std::string DB_HOST;
    const std::string DB_USER;
    const std::string DB_PASS;
    const std::string DB_NAME;
    const std::string DB_PORT;

    /**
     * @brief Cria e retorna uma nova conex�o com o banco de dados MySQL.
     * A conex�o � gerenciada por
     * std::unique_ptr para garantir o fechamento autom�tico.
     * @return Um unique_ptr para a conex�o ou nullptr em caso de falha.
     */
    Connection createConnection() {
        try {
            Connection conn(driver->connect("tcp://" + DB_HOST + ":" + DB_PORT, DB_USER, DB_PASS));
            conn->setSchema(DB_NAME);
            return conn;
        }
        catch (sql::SQLException& e) {
            std::cerr << "Erro de conex�o MySQL: " << e.what() << std::endl;
            std::cerr << "SQLSTATE: " << e.getSQLState() << ", Erro Code: " << e.getErrorCode() << std::endl;
            return nullptr;
        }
    }

public:
    /**
     * @brief Construtor da classe BaseRepository.
     * Carrega as configura��es do banco de dados de um objeto DatabaseConfig.
     * @param config Objeto DatabaseConfig contendo as credenciais.
     */
    BaseRepository(const DatabaseConfig& config)
        : DB_HOST(config.host), DB_USER(config.user), DB_PASS(config.password),
        DB_NAME(config.database), DB_PORT(config.port) {
        try {
            driver = get_driver_instance();
        }
        catch (sql::SQLException& e) {
            std::cerr << "Erro ao obter driver MySQL: " << e.what() << std::endl;
            throw;
        }
    }

    virtual ~BaseRepository() {}

    /**
     * @brief Executa um comando SQL que n�o retorna resultados (e.g., INSERT, UPDATE, DELETE).
     * @param sqlQuery A string SQL a ser executada.
     * @return O n�mero de linhas afetadas pela opera��o ou -1 em caso de erro.
     */
    int execute(const std::string& sqlQuery) {
        std::unique_ptr<sql::Connection> con = createConnection();
        if (!con) {
            std::cerr << "Falha ao conectar para executar: " << sqlQuery << std::endl;
            return -1;
        }
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            return stmt->executeUpdate(sqlQuery);
        }
        catch (sql::SQLException& e) {
            std::cerr << "Erro ao executar comando SQL: " << e.what() << std::endl;
            std::cerr << "SQLSTATE: " << e.getSQLState() << ", Erro Code: " << e.getErrorCode() << std::endl;
            return -1;
        }
    }

    /**
     * @brief Executa uma consulta SQL que retorna um conjunto de resultados (e.g., SELECT).
     * @param sqlQuery A string SQL da consulta.
     * @return Um vetor de vetores de strings (ResultSetData) representando os dados.
     * Retorna um vetor vazio em caso de erro ou se nenhum dado for encontrado.
     */
    DataSet executeQuery(const std::string& sqlQuery) {
        DataSet results;
        std::unique_ptr<sql::Connection> con = createConnection();
        if (!con) {
            std::cerr << "Falha ao conectar para consultar: " << sqlQuery << std::endl;
            return results;
        }
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sqlQuery));

            sql::ResultSetMetaData* meta = res->getMetaData();
            unsigned int numColumns = meta->getColumnCount();

            while (res->next()) {
                Row rowData;
                rowData.reserve(numColumns);
                for (unsigned int i = 1; i <= numColumns; ++i) {
                    rowData.push_back(res->getString(i));
                }
                results.push_back(rowData);
            }
            return results;
        }
        catch (sql::SQLException& e) {
            std::cerr << "Erro ao executar consulta SQL: " << e.what() << std::endl;
            std::cerr << "SQLSTATE: " << e.getSQLState() << ", Erro Code: " << e.getErrorCode() << std::endl;
            return results;
        }
    }

    /**
     * @brief Retorna uma �nica entidade pelo seu ID.
     * @param id O ID da entidade a ser buscada.
     * @return Um std::shared_ptr para a entidade encontrada ou nullptr se n�o for encontrada.
     */
    using EntityObject = std::shared_ptr<T>;
    EntityObject getById(long long id) {
        // Crie uma inst�ncia tempor�ria do tipo T para obter o nome da tabela
        T tempEntity;
        std::string tableName = tempEntity.getTableName();

        // Construa a query SELECT
        std::string sqlQuery = "SELECT ";
        // Para getById, queremos todas as colunas que a entidade sabe preencher.
        // Assumimos que getColumnNamesForSelect() inclui 'id'.
        std::vector<std::string> selectColumns = tempEntity.getColumnNamesForSelect();
        for (size_t i = 0; i < selectColumns.size(); ++i) {
            sqlQuery += selectColumns[i];
            if (i < selectColumns.size() - 1) {
                sqlQuery += ", ";
            }
        }
        sqlQuery += " FROM " + tableName + " WHERE id = " + std::to_string(id) + ";";

        DataSet results = executeQuery(sqlQuery);
        if (results.empty())
        {
            return nullptr; // Entidade n�o encontrada
        }
        // Se uma linha foi encontrada, crie uma nova entidade e preencha-a
        // std::make_shared garante que o objeto seja criado e gerenciado corretamente.
        EntityObject entity = std::make_shared<T>();
        try {
            // O primeiro elemento de results � a �nica linha que esperamos
            entity->fillFromRow(results[0]);
            return entity;
        }
        catch (const std::exception& e) {
            std::cerr << "Erro ao preencher entidade do tipo " << tableName << ": " << e.what() << std::endl;
            return nullptr;
        }
    }


};

#endif