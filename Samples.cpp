// --- Exemplo de Uso ---
#include "BaseRepository.hpp"
// Uma classe derivada de Entity

#include<vector>
std::vector<std::string> listaDeStrings;
std::vector<Entity> entidades;
std::vector<int> listaDeInteiros;

// --- Classes Derivadas (Exemplos) ---

class Product : public Entity {
private:
    std::string name;
    double price;

public:
    // Construtor padr�o � OBRIGAT�RIO para o reposit�rio gen�rico
    Product() : name(""), price(0.0) {}
    Product(const std::string& name, double price)
        : name(name), price(price) {}

    // Getters
    const std::string& getName() const { return name; }
    double getPrice() const { return price; }

    // Setters (OBRIGAT�RIO para o reposit�rio gen�rico)
    void setName(const std::string& newName) { name = newName; }
    void setPrice(double newPrice) { price = newPrice; }

    // Implementa��es dos m�todos virtuais puros
    std::string getTableName() const override { return "products"; }

    std::vector<std::string> getColumnNamesForInsert() const override {
        return { "name", "price" };
    }

    std::vector<std::string> getValuesForInsert() const override {
        return { "'" + name + "'", std::to_string(price) };
    }

    std::vector<std::string> getUpdatePairs() const override {
        return { "name = '" + name + "'", "price = " + std::to_string(price) };
    }

    std::vector<std::string> getColumnNamesForSelect() const override {
        return { "id", "name", "price", "created_at", "updated_at" };
    }

    // M�todo crucial: preencher o objeto a partir de uma linha do ResultSetData
    void fillFromRow(const Row& row) override {
        if (row.size() < 5) { // id, name, price, created_at, updated_at
            throw std::runtime_error("N�mero insuficiente de colunas para preencher Product.");
        }
        setId(std::stoll(row[0])); // Converte string para long long para o ID
        setName(row[1]);
        setPrice(std::stod(row[2])); // Converte string para double
        // Para created_at e updated_at, voc� precisaria de uma convers�o de string para time_t
        // Simplificado aqui, mas em produ��o, use uma biblioteca de data/hora mais robusta
        // Por enquanto, vamos ignorar a convers�o para time_t, mas a string estaria dispon�vel.
        // setCreatedAt(parse_time_t_from_string(row[3]));
        // setUpdatedAt(parse_time_t_from_string(row[4]));
    }

    void displayInfo() const override {
        std::cout << "--- Informa��es do Produto ---" << std::endl;
        std::cout << "ID: " << getId() << std::endl;
        std::cout << "Nome: " << getName() << std::endl;
        std::cout << "Pre�o: R$" << std::fixed << std::setprecision(2) << getPrice() << std::endl;
        std::cout << "Criado em: " << getCreatedAt() << std::endl;
        std::cout << "�ltima Atualiza��o: " << getUpdatedAt() << std::endl;
        std::cout << "-----------------------------" << std::endl;
    }
};

class User : public Entity {
private:
    std::string username;
    std::string email;

public:
    // Construtor padr�o � OBRIGAT�RIO
    User() : username(""), email("") {}
    User(const std::string& username, const std::string& email)
        : username(username), email(email) {}

    // Getters
    const std::string& getUsername() const { return username; }
    const std::string& getEmail() const { return email; }

    // Setters (OBRIGAT�RIO)
    void setUsername(const std::string& newUsername) { username = newUsername; }
    void setEmail(const std::string& newEmail) { email = newEmail; }

    // Implementa��es dos m�todos virtuais puros
    std::string getTableName() const override { return "users"; }

    std::vector<std::string> getColumnNamesForInsert() const override {
        return { "username", "email" };
    }

    std::vector<std::string> getValuesForInsert() const override {
        return { "'" + username + "'", "'" + email + "'" };
    }

    std::vector<std::string> getUpdatePairs() const override {
        return { "username = '" + username + "'", "email = '" + email + "'" };
    }

    std::vector<std::string> getColumnNamesForSelect() const override {
        return { "id", "username", "email", "created_at", "updated_at" };
    }

    // M�todo crucial: preencher o objeto a partir de uma linha do ResultSetData
    void fillFromRow(const Row& row) override {
        if (row.size() < 5) { // id, username, email, created_at, updated_at
            throw std::runtime_error("N�mero insuficiente de colunas para preencher User.");
        }
        setId(std::stoll(row[0]));
        setUsername(row[1]);
        setEmail(row[2]);
        // Para created_at e updated_at, voc� precisaria de uma convers�o
    }

    void displayInfo() const override {
        std::cout << "--- Informa��es do Usu�rio ---" << std::endl;
        std::cout << "ID: " << getId() << std::endl;
        std::cout << "Nome de Usu�rio: " << getUsername() << std::endl;
        std::cout << "Email: " << getEmail() << std::endl;
        std::cout << "Criado em: " << getCreatedAt() << std::endl;
        std::cout << "�ltima Atualiza��o: " << getUpdatedAt() << std::endl;
        std::cout << "-----------------------------" << std::endl;
    }
};

int main() {
    // Cria��o de objetos de classes derivadas
    Product p1("Smart TV 4K", 2500.99);
    p1.displayInfo();

    User u1("johndoe", "john.doe@example.com");
    u1.displayInfo();

    std::cout << "\n--- Modificando entidades ---\n";
    p1.setPrice(2350.00); // Isso deve atualizar o updatedAt
    p1.displayInfo();

    u1.setUsername("johndoe_new"); // Isso deve atualizar o updatedAt
    u1.displayInfo();

    // Comparando entidades
    Product p2("Fone de Ouvido", 150.00);
    std::cout << "\nComparando entidades:\n";
    if (p1 == p2) {
        std::cout << "P1 e P2 s�o a mesma entidade (mesmo ID).\n";
    }
    else {
        std::cout << "P1 e P2 s�o entidades diferentes.\n";
    }

    Product p3("Smart TV 4K", 2500.99); // Ter� um ID diferente de p1
    if (p1 != p3) {
        std::cout << "P1 e P3 s�o entidades diferentes (IDs diferentes).\n";
    }

    // Exemplo de polimorfismo
    std::cout << "\n--- Exemplo de Polimorfismo ---\n";
    std::shared_ptr<Entity> entity1 = std::make_shared<Product>("Webcam Full HD", 80.00);
    std::shared_ptr<Entity> entity2 = std::make_shared<User>("maryjane", "mary.jane@example.com");

    entity1->displayInfo();
    entity2->displayInfo();

    return 0;
}