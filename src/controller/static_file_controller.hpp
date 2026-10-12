#pragma once

#include "controller/http_types.hpp"

namespace http_handler
{
/** @brief Отдаёт файлы из одного каталога, проверяя выход за его границы и символические ссылки. */
class StaticFileController
{
public:
    /** @brief Запоминает корневой каталог; пути разрешаются при обработке запроса. */
    explicit StaticFileController(const std::string_view root):
            m_static_path(root)
    {
    }
    /**
     * @brief Открывает файл либо index.html каталога и определяет Content-Type.
     * @param request HTTP-запрос; семантика методов сохраняет контракт статического сервера.
     * @param uri Декодированный путь без строки запроса.
     * @return Файловый ответ либо ошибка 400/404; поток файла принадлежит возвращённому ответу.
     * @throws std::filesystem::filesystem_error Ошибка доступа к файловой системе.
     * @throws std::runtime_error Файл существует, но открыть его не удалось.
     */
    FileRequestResult Handle(const StringRequest& request, const std::string_view uri) const;

private:
    /** @brief Корневой каталог, за пределы которого нельзя выходить. */
    const fs::path m_static_path;
    /** @brief Сопоставляет расширение MIME-типу; неизвестное получает application/octet-stream. */
    static std::string_view GetFileContentType(const fs::path& path);
    /** @brief Сравнивает компоненты канонических путей, а не их строковые префиксы. */
    static bool IsSubPath(const fs::path& path, const fs::path& base);
};
} // namespace http_handler
