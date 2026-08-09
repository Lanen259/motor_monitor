#include <QtTest/QtTest>
#include "../../../src/databus/Topic.h"

using namespace MotorStudio;

class TestTopicRegistry : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Registry is a singleton — start fresh by testing basic ops
    }

    void testRegisterNewTopic()
    {
        auto& reg = TopicRegistry::instance();
        TopicId id = reg.registerTopic("Ia");
        QVERIFY(id > 0);
        QCOMPARE(reg.topicName(id), std::string("Ia"));
    }

    void testRegisterDuplicateReturnsSameId()
    {
        auto& reg = TopicRegistry::instance();
        TopicId id1 = reg.registerTopic("Speed");
        TopicId id2 = reg.registerTopic("Speed");
        QCOMPARE(id1, id2);
    }

    void testFindExistingTopic()
    {
        auto& reg = TopicRegistry::instance();
        TopicId id = reg.registerTopic("Voltage");
        QCOMPARE(reg.findTopic("Voltage"), id);
    }

    void testFindNonExistentTopic()
    {
        auto& reg = TopicRegistry::instance();
        QCOMPARE(reg.findTopic("NonExistent_12345"), TopicId(0));
    }

    void testRegisterTopicsBatch()
    {
        auto& reg = TopicRegistry::instance();
        std::vector<std::string> names = {"CH1", "CH2", "CH3"};
        auto ids = reg.registerTopics(names);
        QCOMPARE(ids.size(), size_t(3));
        QVERIFY(ids[0] != ids[1]);
        QVERIFY(ids[1] != ids[2]);
    }

    void testAllTopicIds()
    {
        auto& reg = TopicRegistry::instance();
        auto ids = reg.allTopicIds();
        QVERIFY(ids.size() > 0);
    }

    void testChannelDescriptor()
    {
        auto& reg = TopicRegistry::instance();
        ChannelDescriptor desc;
        desc.name = "Torque";
        desc.unit = "N.m";
        desc.dataType = "float";
        desc.scale = 1.0f;
        desc.offset = 0.0f;

        TopicId id = reg.registerTopic(desc);
        auto retrieved = reg.descriptor(id);
        QCOMPARE(retrieved.name, std::string("Torque"));
        QCOMPARE(retrieved.unit, std::string("N.m"));
        QCOMPARE(retrieved.scale, 1.0f);
    }

    void testCount()
    {
        auto& reg = TopicRegistry::instance();
        size_t before = reg.count();
        reg.registerTopic("UniqueTopic_XYZ");
        QCOMPARE(reg.count(), before + 1);
    }

    void testTopicNameForInvalidId()
    {
        auto& reg = TopicRegistry::instance();
        std::string name = reg.topicName(99999);
        QVERIFY(name.empty());
    }

    // WI-005: re-registering same name with different descriptor must UPDATE, not just return ID
    void testRegisterDescriptorUpdateExisting()
    {
        auto& reg = TopicRegistry::instance();

        // First registration with initial descriptor
        ChannelDescriptor desc1;
        desc1.name = "TestUpdate";
        desc1.unit = "A";
        desc1.dataType = "float";
        desc1.scale = 0.5f;
        desc1.offset = 10.0f;

        TopicId id1 = reg.registerTopic(desc1);
        QVERIFY(id1 > 0);
        QCOMPARE(reg.descriptor(id1).unit, std::string("A"));
        QCOMPARE(reg.descriptor(id1).scale, 0.5f);
        QCOMPARE(reg.descriptor(id1).offset, 10.0f);

        // Re-register with UPDATED descriptor — same name, different fields
        ChannelDescriptor desc2;
        desc2.name = "TestUpdate";  // same name
        desc2.unit = "RPM";
        desc2.dataType = "float";
        desc2.scale = 2.0f;
        desc2.offset = -5.0f;

        TopicId id2 = reg.registerTopic(desc2);
        QCOMPARE(id2, id1);  // MUST return same ID

        // Descriptor MUST be updated
        auto retrieved = reg.descriptor(id1);
        QCOMPARE(retrieved.name, std::string("TestUpdate"));
        QCOMPARE(retrieved.unit, std::string("RPM"));
        QCOMPARE(retrieved.scale, 2.0f);
        QCOMPARE(retrieved.offset, -5.0f);
    }

    // WI-005: Re-registering by topicId must support RENAME (old name removed, new name mapped)
    void testRegisterDescriptorRename()
    {
        auto& reg = TopicRegistry::instance();

        // Register a channel with an initial name
        ChannelDescriptor desc1;
        desc1.name = "RenameOriginal";
        desc1.unit = "V";
        desc1.dataType = "float";
        desc1.color = 0xFF00FF00;  // green

        TopicId id1 = reg.registerTopic(desc1);
        QVERIFY(id1 > 0);
        QCOMPARE(reg.findTopic("RenameOriginal"), id1);

        // Simulate what ChannelConfigDialog does: fetch descriptor, modify, re-register
        auto desc2 = reg.descriptor(id1);   // has topicId set
        desc2.name = "RenameTarget";         // new name
        desc2.unit = "A";                    // new unit
        desc2.color = 0xFFFF0000;            // new color (red)

        TopicId id2 = reg.registerTopic(desc2);
        QCOMPARE(id2, id1);  // MUST return same ID

        // New name must resolve
        QCOMPARE(reg.findTopic("RenameTarget"), id1);

        // Old name must be gone
        QCOMPARE(reg.findTopic("RenameOriginal"), TopicId(0));

        // Descriptor must reflect all updates
        auto retrieved = reg.descriptor(id1);
        QCOMPARE(retrieved.name, std::string("RenameTarget"));
        QCOMPARE(retrieved.unit, std::string("A"));
        QCOMPARE(retrieved.color, 0xFFFF0000u);
    }
};

QTEST_MAIN(TestTopicRegistry)
#include "test_topic_registry.moc"
