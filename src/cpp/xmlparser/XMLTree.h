#ifndef _XML_TREE_
#define _XML_TREE_

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace eprosima {
namespace fastdds {
namespace xmlparser {

enum class NodeType
{
    PROFILES,
    PARTICIPANT,
    PUBLISHER,
    SUBSCRIBER,
    RTPS,
    QOS_PROFILE,
    APPLICATION,
    TYPE,
    TOPIC,
    DATA_WRITER,
    DATA_READER,
    ROOT,
    TYPES,
    LOG,
    REQUESTER,
    REPLIER,
    LIBRARY_SETTINGS,
    DOMAINPARTICIPANT_FACTORY
};

class BaseNode
{
public:

    BaseNode(
            NodeType type)
        : data_type_(type)
        , parent_(nullptr)
    {
    }

    virtual ~BaseNode() = default;

    BaseNode(
            const BaseNode&) = delete;
    BaseNode& operator =(
            const BaseNode&) = delete;

    BaseNode(
            BaseNode&&) = default;
    BaseNode& operator =(
            BaseNode&&) = default;

    NodeType getType() const
    {
        return data_type_;
    }

    void addChild(
            std::unique_ptr<BaseNode> child)
    {
        child->setParent(this);
        children.push_back(std::move(child));
    }

    bool removeChild(
            const size_t& index)
    {
        if (children.size() > index)
        {
            children.erase(children.begin() + index);
            return true;
        }
        else
        {
            return false;
        }
    }

    BaseNode* getChild(
            const size_t& index) const
    {
        if (children.empty())
        {
            return nullptr;
        }
        return children[index].get();
    }

    BaseNode* getParent() const
    {
        return parent_;
    }

    void setParent(
            BaseNode* parent)
    {
        parent_ = parent;
    }

    size_t getNumChildren() const
    {
        return children.size();
    }

    std::vector<std::unique_ptr<BaseNode>>& getChildren()
    {
        return children;
    }

private:

    NodeType data_type_;
    BaseNode* parent_;
    std::vector<std::unique_ptr<BaseNode>> children;
};

template <class T>
class DataNode : public BaseNode
{
public:

    DataNode(
            NodeType type);
    DataNode(
            NodeType type,
            std::unique_ptr<T> data);
    virtual ~DataNode();

    DataNode(
            const DataNode&) = delete;
    DataNode& operator =(
            const DataNode&) = delete;

    DataNode(
            DataNode&&)            = default;
    DataNode& operator =(
            DataNode&&) = default;

    T* get() const;
    std::unique_ptr<T> getData();
    void setData(
            std::unique_ptr<T> data);

    void addAttribute(
            const std::string& name,
            const std::string& value);
    const std::map<std::string, std::string>& getAttributes();

private:

    std::map<std::string, std::string> attributes_;
    std::unique_ptr<T> data_;
};

template <class T>
DataNode<T>::DataNode(
        NodeType type)
    : BaseNode(type)
    , attributes_()
    , data_(nullptr)
{
}

template <class T>
DataNode<T>::DataNode(
        NodeType type,
        std::unique_ptr<T> data)
    : BaseNode(type)
    , attributes_()
    , data_(std::move(data))
{
}

template <class T>
DataNode<T>::~DataNode()
{
}

template <class T>
T* DataNode<T>::get() const
{
    return data_.get();
}

template <class T>
std::unique_ptr<T> DataNode<T>::getData()
{
    return std::move(data_);
}

template <class T>
void DataNode<T>::setData(
        std::unique_ptr<T> data)
{
    data_ = std::move(data);
}

template <class T>
void DataNode<T>::addAttribute(
        const std::string& name,
        const std::string& value)
{
    attributes_[name] = value;
}

template <class T>
const std::map<std::string, std::string>& DataNode<T>::getAttributes()
{
    return attributes_;
}

} // namespace xmlparser
} // namespace fastdds
} // namespace eprosima
#endif // !_XML_TREE_
