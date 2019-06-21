#pragma once

#include "core/Base.h"
#include "core/FileSystem.h"
#include <algorithm>
#include <cstdio>

class IStream {
public:
    virtual ~IStream() {
        // Derived streams should implement destructor and call CloseImpl() directly
    }

    void Close() { CloseImpl(); }

    bool IsOpen() const { return IsOpenImpl(); }

    void SetPos(size_t pos) { SetPosImpl(pos); }

    template <typename T>
    bool ReadValue(T& value) {
        return ReadImpl(&value, sizeof(T), 1) == 1;
    }

    template <typename T>
    bool Read(T* destBuffer, size_t count = 1) {
        return ReadImpl(destBuffer, sizeof(T), count) == count;
    }

    template <typename T>
    size_t WriteValue(const T& value) {
        return WriteImpl(&value, sizeof(T), 1);
    }

    template <typename T>
    size_t Write(T* srcBuffer, size_t count = 1) {
        return WriteImpl(srcBuffer, sizeof(T), count);
    }

    void Printf(const char* format, ...);

protected:
    virtual void CloseImpl() = 0;
    virtual bool IsOpenImpl() const = 0;
    // Returns total number of elements read, or 0 if EOF/ERROR
    virtual size_t ReadImpl(void* dest, size_t elemSize, size_t count) = 0;
    virtual size_t WriteImpl(const void* source, size_t elemSize, size_t count) = 0;
    virtual bool SetPosImpl(size_t pos) = 0;
};

// Streams to/from file on disk
class FileStream final : public IStream {
public:
    FileStream()
        : m_file(nullptr) {}

    FileStream(const char* name, const char* mode)
        : m_file(nullptr) {
        if (!Open(name, mode))
            FAIL_MSG("Failed to open file: %s", name);
    }

    ~FileStream() override { CloseImpl(); }

    bool Open(const char* name, const char* mode) {
        Close();
        m_file = fopen(name, mode);
        return m_file != nullptr;
    }

    bool Open(const fs::path& path, const char* mode) { return Open(path.string().c_str(), mode); }

    FILE* Get() { return m_file; }
    const FILE* Get() const { return m_file; }

protected:
    void CloseImpl() override {
        if (m_file) {
            fclose(m_file);
            m_file = nullptr;
        }
    }

    bool IsOpenImpl() const override { return m_file != nullptr; }

    size_t ReadImpl(void* dest, size_t elemSize, size_t count) override {
        return fread(dest, elemSize, count, m_file);
    }

    size_t WriteImpl(const void* source, size_t elemSize, size_t count) override {
        return fwrite(source, elemSize, count, m_file);
    }

    bool SetPosImpl(size_t pos) override {
        return fseek(m_file, checked_static_cast<long>(pos), 0) == 0;
    }

private:
    FILE* m_file;
};

// Streams to/from a fixed-size block of memory
class MemoryStream final : public IStream {
public:
    ~MemoryStream() override { CloseImpl(); }

    void Open(uint8_t* buffer, size_t size) {
        m_buffer = buffer;
        m_curr = m_buffer;
        m_size = size;
    }

protected:
    uint8_t* End() { return m_curr + m_size; }

    void CloseImpl() override {
        m_curr = nullptr;
        // We leave buffer alone in case it gets reused. Memory will be reclaimed when stream is
        // destroyed.
    }

    bool IsOpenImpl() const override { return m_curr != nullptr; }

    size_t ReadImpl(void* dest, size_t elemSize, size_t count) override {
        //@TODO: instead of asserting, read what we can and return amount read
        const size_t size = elemSize * count;
        assert(m_curr + size <= End());
        std::copy_n(m_curr, size, (uint8_t*)dest);
        m_curr += size;
        return count;
    }

    size_t WriteImpl(const void* source, size_t elemSize, size_t count) override {
        //@TODO: instead of asserting, write what we can and return amount written
        const size_t size = elemSize * count;
        assert(m_curr + size <= End());
        std::copy_n((uint8_t*)source, size, m_curr);
        m_curr += size;
        return size;
    }

    bool SetPosImpl(size_t pos) override {
        //@TODO: instead of asserting, return false if we can't set pos
        assert(pos < m_size);
        m_curr = m_buffer + pos;
        return true;
    }

private:
    uint8_t* m_buffer;
    uint8_t* m_curr;
    size_t m_size;
};

// Stream that counts the number of bytes that would be written
class ByteCounterStream final : public IStream {
public:
    ByteCounterStream()
        : m_size(0) {}

    ~ByteCounterStream() override { CloseImpl(); }

    size_t GetStreamSize() const { return m_size; }

protected:
    void CloseImpl() override {}

    bool IsOpenImpl() const override { return true; }

    size_t ReadImpl(void* /*dest*/, size_t /*elemSize*/, size_t /*count*/) override {
        assert(false); // For couting output streams only
        return 0;
    }

    size_t WriteImpl(const void* /*source*/, size_t elemSize, size_t count) override {
        m_size += (elemSize * count);
        return (elemSize * count);
    }

    bool SetPosImpl(size_t /*pos*/) override {
        assert(false); // Not supported
        return false;
    }

private:
    size_t m_size;
};
