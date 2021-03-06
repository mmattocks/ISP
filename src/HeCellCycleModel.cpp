#include "HeCellCycleModel.hpp"

HeCellCycleModel::HeCellCycleModel() :
        AbstractSimpleCellCycleModel(), mKillSpecified(false), mDeterministic(false), mOutput(false), mEventStartTime(
                24.0), mSequenceSampler(false), mSeqSamplerLabelSister(false), mDebug(false), mTimeID(), mVarIDs(), mDebugWriter(), mTiLOffset(
                0.0), mGammaShift(4.0), mGammaShape(2.0), mGammaScale(1.0), mSisterShiftWidth(1), mMitoticModePhase2(
                8.0), mMitoticModePhase3(15.0), mPhaseShiftWidth(2.0), mPhase1PP(1.0), mPhase1PD(0.0), mPhase2PP(0.2), mPhase2PD(
                0.4), mPhase3PP(0.2), mPhase3PD(0.0), mMitoticMode(0), mSeed(0), mTimeDependentCycleDuration(false), mPeakRateTime(), mIncreasingRateSlope(), mDecreasingRateSlope(), mBaseGammaScale()
{
    mReadyToDivide = true; //He model begins with a first division
}

HeCellCycleModel::HeCellCycleModel(const HeCellCycleModel& rModel) :
        AbstractSimpleCellCycleModel(rModel), mKillSpecified(rModel.mKillSpecified), mDeterministic(
                rModel.mDeterministic), mOutput(rModel.mOutput), mEventStartTime(rModel.mEventStartTime), mSequenceSampler(
                rModel.mSequenceSampler), mSeqSamplerLabelSister(rModel.mSeqSamplerLabelSister), mDebug(rModel.mDebug), mTimeID(
                rModel.mTimeID), mVarIDs(rModel.mVarIDs), mDebugWriter(rModel.mDebugWriter), mTiLOffset(
                rModel.mTiLOffset), mGammaShift(rModel.mGammaShift), mGammaShape(rModel.mGammaShape), mGammaScale(
                rModel.mGammaScale), mSisterShiftWidth(rModel.mSisterShiftWidth), mMitoticModePhase2(
                rModel.mMitoticModePhase2), mMitoticModePhase3(rModel.mMitoticModePhase3), mPhaseShiftWidth(
                rModel.mPhaseShiftWidth), mPhase1PP(rModel.mPhase1PP), mPhase1PD(rModel.mPhase1PD), mPhase2PP(
                rModel.mPhase2PP), mPhase2PD(rModel.mPhase2PD), mPhase3PP(rModel.mPhase3PP), mPhase3PD(
                rModel.mPhase3PD), mMitoticMode(rModel.mMitoticMode), mSeed(rModel.mSeed), mTimeDependentCycleDuration(
                rModel.mTimeDependentCycleDuration), mPeakRateTime(rModel.mPeakRateTime), mIncreasingRateSlope(
                rModel.mIncreasingRateSlope), mDecreasingRateSlope(rModel.mDecreasingRateSlope), mBaseGammaScale(
                rModel.mBaseGammaScale)
{
}

AbstractCellCycleModel* HeCellCycleModel::CreateCellCycleModel()
{
    return new HeCellCycleModel(*this);
}

void HeCellCycleModel::SetCellCycleDuration()
{
    RandomNumberGenerator* p_random_number_generator = RandomNumberGenerator::Instance();

    /**************************************
     * CELL CYCLE DURATION RANDOM VARIABLE
     *************************************/

    if (!mTimeDependentCycleDuration) //Normal operation, cell cycle length stays constant
    {
        //He cell cycle length determined by shifted gamma distribution reflecting 4 hr refractory period followed by gamma pdf
        mCellCycleDuration = mGammaShift + p_random_number_generator->GammaRandomDeviate(mGammaShape, mGammaScale);
    }

    /****
     * Variable cycle length
     * Give -ve mIncreasingRateSlope and +ve mDecreasingRateSlope,
     * cell cycle length linearly declines (increasing rate), then increases, switching at mPeakRateTime
     ****/
    else
    {
        double currTime = SimulationTime::Instance()->GetTime();
        if (currTime <= mPeakRateTime)
        {
            mGammaScale = std::max((mBaseGammaScale - currTime * mIncreasingRateSlope), .0000000000001);
        }
        if (currTime > mPeakRateTime)
        {
            mGammaScale = std::max(
                    ((mBaseGammaScale - mPeakRateTime * mIncreasingRateSlope)
                            + (mBaseGammaScale + (currTime - mPeakRateTime) * mDecreasingRateSlope)),
                    .0000000000001);
        }
        mCellCycleDuration = mGammaShift + p_random_number_generator->GammaRandomDeviate(mGammaShape, mGammaScale);
    }

}

void HeCellCycleModel::ResetForDivision()
{
    /****************************************************
     * TIME IN LINEAGE DEPENDENT MITOTIC MODE PHASE RULES
     * **************************************************/
    RandomNumberGenerator* p_random_number_generator = RandomNumberGenerator::Instance();

    double currentTiL = SimulationTime::Instance()->GetTime() + mTiLOffset;

    /*Rule logic defaults to phase 1 behaviour, checks for currentTiL > phaseBoundaries and changes
     currentPhase and subsequently mMitoticMode as appropriate*/
    unsigned currentPhase = 1;
    mMitoticMode = 0;

    //Check time in lineage and determine current mitotic mode phase
    /**************
     * Phase boundary & deterministic mitotic mode rules
     **************/
    if (currentTiL > mMitoticModePhase2 && currentTiL < mMitoticModePhase3)
    {
        //if current TiL is > phase 2 boundary time, set the currentPhase appropriately
        currentPhase = 2;

        //if deterministic mode is enabled, PD divisions are guaranteed unless this is an Ath5 morphant
        if (mDeterministic)
        {
            mMitoticMode = 1; //0=PP;1=PD;2=DD
            if (mpCell->HasCellProperty<Ath5Mo>()) //Ath5 morphants undergo PP rather than PD divisions in 80% of cases
            {
                double ath5RV = p_random_number_generator->ranf();
                if (ath5RV <= .8)
                {
                    mMitoticMode = 0;
                }
            }
        }
    }

    if (currentTiL > mMitoticModePhase3)
    {
        //if current TiL is > phase 3 boundary time, set the currentPhase appropriately
        currentPhase = 3;
        if (mDeterministic)
        {
            //if deterministic mode is enabled, DD divisions are guaranteed
            mMitoticMode = 2;
        }
    }

    /******************************
     * MITOTIC MODE RANDOM VARIABLE
     ******************************/
    //initialise mitoticmode random variable, set mitotic mode appropriately after comparing to mode probability matrix
    double mitoticModeRV = p_random_number_generator->ranf(); //0-1 evenly distributed RV

    if (!mDeterministic)
    {
        //construct 3x2 matrix of mode probabilities arranged by phase
        double modeProbabilityMatrix[3][2] = { { mPhase1PP, mPhase1PD }, { mPhase2PP, mPhase2PD }, { mPhase3PP,
                                                                                                     mPhase3PD } };

        //if the RV is > currentPhasePP && <= currentPhasePD, change mMitoticMode from PP to PD
        if (mitoticModeRV > modeProbabilityMatrix[currentPhase - 1][0]
                && mitoticModeRV
                        <= modeProbabilityMatrix[currentPhase - 1][0] + modeProbabilityMatrix[currentPhase - 1][1])
        {
            mMitoticMode = 1;
            if (mpCell->HasCellProperty<Ath5Mo>()) //Ath5 morphants undergo PP rather than PD divisions in 80% of cases
            {
                double ath5RV = p_random_number_generator->ranf();
                if (ath5RV <= .8)
                {
                    mMitoticMode = 0;
                }
            }
        }
        //if the RV is > currentPhasePP + currentPhasePD, change mMitoticMode from PP to DD
        if (mitoticModeRV > modeProbabilityMatrix[currentPhase - 1][0] + modeProbabilityMatrix[currentPhase - 1][1])
        {
            mMitoticMode = 2;
        }
    }

    /****************
     * Write mitotic event to relevant files
     * *************/
    if (mDebug)
    {
        WriteDebugData(currentTiL, currentPhase, mitoticModeRV);
    }

    if (mOutput)
    {
        WriteModeEventOutput();
    }

    //set new cell cycle length (will be overwritten with DBL_MAX for DD divisions)
    AbstractSimpleCellCycleModel::ResetForDivision();

    /****************
     * Symmetric postmitotic specification rule
     * -(asymmetric postmitotic rule specified in InitialiseDaughterCell();)
     * *************/
    if (mMitoticMode == 2)
    {
        boost::shared_ptr<AbstractCellProperty> p_PostMitoticType =
                mpCell->rGetCellPropertyCollection().GetCellPropertyRegistry()->Get<DifferentiatedCellProliferativeType>();
        mpCell->SetCellProliferativeType(p_PostMitoticType);
        mCellCycleDuration = DBL_MAX;

        if(mKillSpecified)
        {
            mpCell->Kill();
        }
    }

    /******************
     * SEQUENCE SAMPLER
     ******************/
    //if the sequence sampler has been turned on, check for the label & write mitotic mode to log
    //50% chance of each daughter cell from a mitosis inheriting the label
    if (mSequenceSampler)
    {
        if (mpCell->HasCellProperty<CellLabel>())
        {
            (*LogFile::Instance()) << mMitoticMode;
            double labelRV = p_random_number_generator->ranf();
            if (labelRV <= .5)
            {
                mSeqSamplerLabelSister = true;
                mpCell->RemoveCellProperty<CellLabel>();
            }
            else
            {
                mSeqSamplerLabelSister = false;
            }
        }
        else
        {
            //prevents lost-label cells from labelling their progeny
            mSeqSamplerLabelSister = false;
        }
    }
}

void HeCellCycleModel::Initialise()
{
    boost::shared_ptr<AbstractCellProperty> p_Transit =
            mpCell->rGetCellPropertyCollection().GetCellPropertyRegistry()->Get<TransitCellProliferativeType>();
    mpCell->SetCellProliferativeType(p_Transit);

    if (mTiLOffset == 0) //the "regular" case, set cycle duration normally
    {
        SetCellCycleDuration();
    }

    if (mTiLOffset < 0) //these cells are offspring of Wan stem cells
    {
        mReadyToDivide = false;
        SetCellCycleDuration();
    }

    if (mTiLOffset > 0.0) //if the TiL is > 0, the first division has already occurred
    {
        mReadyToDivide = false;

        RandomNumberGenerator* p_random_number_generator = RandomNumberGenerator::Instance();

        /**
         * This calculation "runs time forward" by subtracting appropriately generated cell lengths from TiLOffset
         * Ultimately c is subtracted from a final cell length calculation to give the appropriate reduced cycle length
         **/

        double c = mTiLOffset;
        while (c > 0)
        {
            c = c - (mGammaShift + p_random_number_generator->GammaRandomDeviate(mGammaShape, mGammaScale));
        }

        mCellCycleDuration = (mGammaShift + p_random_number_generator->GammaRandomDeviate(mGammaShape, mGammaScale))
                + c;
    }

}

void HeCellCycleModel::InitialiseDaughterCell()
{
    RandomNumberGenerator* p_random_number_generator = RandomNumberGenerator::Instance();

    /************
     * PD-type division & shifted sister cycle length & boundary adjustments
     **********/

    if (mMitoticMode == 1) //RPC becomes specified retinal neuron in asymmetric PD mitosis
    {
        boost::shared_ptr<AbstractCellProperty> p_PostMitoticType =
                mpCell->rGetCellPropertyCollection().GetCellPropertyRegistry()->Get<DifferentiatedCellProliferativeType>();
        mpCell->SetCellProliferativeType(p_PostMitoticType);
        mCellCycleDuration = DBL_MAX;

        if(mKillSpecified)
        {
            mpCell->Kill();
        }
    }

    //daughter cell's mCellCycleDuration is copied from parent; modified by a normally distributed shift if it remains proliferative
    if (mMitoticMode == 0)
    {
        double sisterShift = p_random_number_generator->NormalRandomDeviate(0, mSisterShiftWidth); //random variable mean 0 SD 1 by default
        mCellCycleDuration = std::max(mGammaShift, mCellCycleDuration + sisterShift); // sister shift respects 4 hour refractory period
    }

    //deterministic model phase boundary division shift for daughter cells
    if (mDeterministic)
    {
        //shift phase boundaries to reflect error in "timer" after division
        double phaseShift = p_random_number_generator->NormalRandomDeviate(0, mPhaseShiftWidth);
        mMitoticModePhase2 = mMitoticModePhase2 + phaseShift;
        mMitoticModePhase3 = mMitoticModePhase3 + phaseShift;
    }

    /******************
     * SEQUENCE SAMPLER
     ******************/
    if (mSequenceSampler)
    {
        if (mSeqSamplerLabelSister)
        {
            boost::shared_ptr<AbstractCellProperty> p_label_type =
                    mpCell->rGetCellPropertyCollection().GetCellPropertyRegistry()->Get<CellLabel>();
            mpCell->AddCellProperty(p_label_type);
            mSeqSamplerLabelSister = false;
        }
        else
        {
            mpCell->RemoveCellProperty<CellLabel>();
        }
    }

    if (mMitoticMode == 2 && mKillSpecified) mpCell->Kill();
}

void HeCellCycleModel::SetModelParameters(double tiLOffset, double mitoticModePhase2, double mitoticModePhase3,
                                          double phase1PP, double phase1PD, double phase2PP, double phase2PD,
                                          double phase3PP, double phase3PD, double gammaShift, double gammaShape,
                                          double gammaScale, double sisterShift)
{
    mTiLOffset = tiLOffset;
    mMitoticModePhase2 = mitoticModePhase2;
    mMitoticModePhase3 = mitoticModePhase3;
    mPhase1PP = phase1PP;
    mPhase1PD = phase1PD;
    mPhase2PP = phase2PP;
    mPhase2PD = phase2PD;
    mPhase3PP = phase3PP;
    mPhase3PD = phase3PD;
    mGammaShift = gammaShift;
    mGammaShape = gammaShape;
    mGammaScale = gammaScale;
    mSisterShiftWidth = sisterShift;
}

void HeCellCycleModel::SetDeterministicMode(double tiLOffset, double mitoticModePhase2, double mitoticModePhase3,
                                            double phaseShiftWidth, double gammaShift, double gammaShape,
                                            double gammaScale, double sisterShift)
{
    mDeterministic = true;
    mTiLOffset = tiLOffset;
    mMitoticModePhase2 = mitoticModePhase2;
    mMitoticModePhase3 = mitoticModePhase3;
    mPhaseShiftWidth = phaseShiftWidth;
    mGammaShift = gammaShift;
    mGammaShape = gammaShape;
    mGammaScale = gammaScale;
    mSisterShiftWidth = sisterShift;
}

void HeCellCycleModel::SetTimeDependentCycleDuration(double peakRateTime, double increasingSlope,
                                                     double decreasingSlope)
{
    mTimeDependentCycleDuration = true;
    mPeakRateTime = peakRateTime;
    mIncreasingRateSlope = increasingSlope;
    mDecreasingRateSlope = decreasingSlope;
    mBaseGammaScale = mGammaScale;
}

void HeCellCycleModel::EnableKillSpecified()
{
    mKillSpecified = true;
}

void HeCellCycleModel::EnableModeEventOutput(double eventStart, unsigned seed)
{
    mOutput = true;
    mEventStartTime = eventStart;
    mSeed = seed;
}

void HeCellCycleModel::WriteModeEventOutput()
{
    double currentTime = SimulationTime::Instance()->GetTime() + mEventStartTime;
    CellPtr currentCell = GetCell();
    double currentCellID = (double) currentCell->GetCellId();
    (*LogFile::Instance()) << currentTime << "\t" << mSeed << "\t" << currentCellID << "\t" << mMitoticMode << "\n";
}

void HeCellCycleModel::EnableSequenceSampler()
{
    mSequenceSampler = true;
    boost::shared_ptr<AbstractCellProperty> p_label_type =
            mpCell->rGetCellPropertyCollection().GetCellPropertyRegistry()->Get<CellLabel>();
    mpCell->AddCellProperty(p_label_type);
}

void HeCellCycleModel::PassDebugWriter(boost::shared_ptr<ColumnDataWriter> debugWriter, int timeID,
                                       std::vector<int> varIDs)
{
    mDebug = true;
    mDebugWriter = debugWriter;
    mTimeID = timeID;
    mVarIDs = varIDs;
}

void HeCellCycleModel::EnableModelDebugOutput(boost::shared_ptr<ColumnDataWriter> debugWriter)
{
    mDebug = true;
    mDebugWriter = debugWriter;

    mTimeID = mDebugWriter->DefineUnlimitedDimension("Time", "h");

    mVarIDs.push_back(mDebugWriter->DefineVariable("CellID", "No"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("TiL", "h"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("CycleDuration", "h"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("Phase2Boundary", "h"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("Phase3Boundary", "h"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("Phase", "No"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("MitoticModeRV", "Percentile"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("MitoticMode", "Mode"));
    mVarIDs.push_back(mDebugWriter->DefineVariable("Label", "binary"));

    mDebugWriter->EndDefineMode();
}

void HeCellCycleModel::WriteDebugData(double currentTiL, unsigned phase, double mitoticModeRV)
{
    double currentTime = SimulationTime::Instance()->GetTime();
    double currentCellID = mpCell->GetCellId();
    unsigned label = 0;
    if (mpCell->HasCellProperty<CellLabel>()) label = 1;

    mDebugWriter->PutVariable(mTimeID, currentTime);
    mDebugWriter->PutVariable(mVarIDs[0], currentCellID);
    mDebugWriter->PutVariable(mVarIDs[1], currentTiL);
    mDebugWriter->PutVariable(mVarIDs[2], mCellCycleDuration);
    mDebugWriter->PutVariable(mVarIDs[3], mMitoticModePhase2);
    mDebugWriter->PutVariable(mVarIDs[4], mMitoticModePhase3);
    mDebugWriter->PutVariable(mVarIDs[5], phase);
    if (!mDeterministic)
    {
        mDebugWriter->PutVariable(mVarIDs[6], mitoticModeRV);
    }
    mDebugWriter->PutVariable(mVarIDs[7], mMitoticMode);
    if (mSequenceSampler)
    {
        mDebugWriter->PutVariable(mVarIDs[8], label);
    }
    mDebugWriter->AdvanceAlongUnlimitedDimension();
}

/******************
 * UNUSED FUNCTIONS (required for class nonvirtuality, do not remove)
 ******************/

double HeCellCycleModel::GetAverageTransitCellCycleTime()
{
    return (0.0);
}

double HeCellCycleModel::GetAverageStemCellCycleTime()
{
    return (0.0);
}

void HeCellCycleModel::OutputCellCycleModelParameters(out_stream& rParamsFile)
{
    *rParamsFile << "\t\t\t<CellCycleDuration>" << mCellCycleDuration << "</CellCycleDuration>\n";

    // Call method on direct parent class
    AbstractSimpleCellCycleModel::OutputCellCycleModelParameters(rParamsFile);
}

// Serialization for Boost >= 1.36
#include "SerializationExportWrapperForCpp.hpp"
CHASTE_CLASS_EXPORT(HeCellCycleModel)
