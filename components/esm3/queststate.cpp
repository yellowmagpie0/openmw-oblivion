#include "queststate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"

namespace ESM
{

    void QuestState::load(ESMReader& esm)
    {
        mTopic = esm.getHNRefId("YETO");
        esm.getHNOT(mState, "QSTA");
        esm.getHNOT(mFinished, "QFIN");
    }

    void QuestState::save(ESMWriter& esm) const
    {
        esm.writeHNRefId("YETO", mTopic);
        esm.writeHNT("QSTA", mState);
        esm.writeHNT("QFIN", mFinished);
    }

    void TrackedQuest::load(ESMReader& esm)
    {
        mQuest = FormKey::deserialize(esm.getHNString("ID__"));
    }

    void TrackedQuest::save(ESMWriter& esm) const
    {
        esm.writeHNString("ID__", mQuest.serialize());
    }

}
