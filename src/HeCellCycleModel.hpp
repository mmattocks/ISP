#ifndef HECELLCYCLEMODEL_HPP_
#define HECELLCYCLEMODEL_HPP_

#include "AbstractSimpleCellCycleModel.hpp"
#include "RandomNumberGenerator.hpp"
#include "Cell.hpp"
#include "TransitCellProliferativeType.hpp"
#include "DifferentiatedCellProliferativeType.hpp"
#include "SmartPointers.hpp"
#include "ColumnDataWriter.hpp"
#include "LogFile.hpp"
#include "CellLabel.hpp"
#include "HeAth5Mo.hpp"

/***********************************
 * HE CELL CYCLE MODEL
 * As described in He et al. 2012 [He2012] doi: 10.1016/j.neuron.2012.06.033
 *
 * USE: By default, HeCellCycleModels are constructed with the parameter fit reported in [He2012].
 * In normal use, the model steps through three phases of mitotic mode probability parameterisation.
 * PP = symmetric proliferative mitotic mode, both progeny remain mitotic
 * PD = asymmetric proliferative mitotic mode, one progeny exits the cell cycle and differentiates
 * DD = symmetric differentiative mitotic mode, both progeny exit the cell cycle and differentiate
 *
 * Change default model parameters with SetModelParameters(<params>);
 * Enable deterministic model alternative with EnableDeterministicMode(<params>);
 *
 * 2 per-model-event output modes:
 * EnableModeEventOutput() enables mitotic mode event logging-all cells will write to the singleton log file
 * EnableModelDebugOutput() enables more detailed debug output, each seed will have its own file written to
 * by a ColumnDataWriter passed to it from the test
 * (eg. by the SetupDebugOutput helper function in the project simulator)
 *
 * 1 mitotic-event-sequence sampler (only samples one "path" through the lineage):
 * EnableSequenceSampler() - one "sequence" of progenitors writes mitotic event type to a string in the singleton log file
 *
 ************************************/

class HeCellCycleModel : public AbstractSimpleCellCycleModel
{
    friend class TestSimpleCellCycleModels;

private:

    /** Needed for serialization. */
    friend class boost::serialization::access;
    /**
     * Archive the cell-cycle model and random number generator, never used directly - boost uses this.
     *
     * @param archive the archive
     * @param version the current version of this class
     */
    template<class Archive>
    void serialize(Archive & archive, const unsigned int version)
    {
        archive & boost::serialization::base_object<AbstractSimpleCellCycleModel>(*this);

        SerializableSingleton<RandomNumberGenerator>* p_wrapper =
                RandomNumberGenerator::Instance()->GetSerializationWrapper();
        archive & p_wrapper;
        archive & mCellCycleDuration;
    }

    //Private write functions for models
    void WriteModeEventOutput();
    void WriteDebugData(double currTiL, unsigned phase, double percentile);

protected:
    //mode/output variables
    bool mKillSpecified;
    bool mDeterministic;
    bool mOutput;
    double mEventStartTime;
    bool mSequenceSampler;
    bool mSeqSamplerLabelSister;
    //debug writer stuff
    bool mDebug;
    int mTimeID;
    std::vector<int> mVarIDs;
    boost::shared_ptr<ColumnDataWriter> mDebugWriter;
    //model parameters and state memory vars
    double mTiLOffset;
    double mGammaShift;
    double mGammaShape;
    double mGammaScale;
    double mSisterShiftWidth;
    double mMitoticModePhase2;
    double mMitoticModePhase3;
    double mPhaseShiftWidth;
    double mPhase1PP;
    double mPhase1PD;
    double mPhase2PP;
    double mPhase2PD;
    double mPhase3PP;
    double mPhase3PD;
    unsigned mMitoticMode;
    unsigned mSeed;
    bool mTimeDependentCycleDuration;
    double mPeakRateTime;
    double mIncreasingRateSlope;
    double mDecreasingRateSlope;
    double mBaseGammaScale;

    /**
     * Protected copy-constructor for use by CreateCellCycleModel().
     *
     * The only way for external code to create a copy of a cell cycle model
     * is by calling that method, to ensure that a model of the correct subclass is created.
     * This copy-constructor helps subclasses to ensure that all member variables are correctly copied when this happens.
     *
     * This method is called by child classes to set member variables for a daughter cell upon cell division.
     * Note that the parent cell cycle model will have had ResetForDivision() called just before CreateCellCycleModel() is called,
     * so performing an exact copy of the parent is suitable behaviour. Any daughter-cell-specific initialisation
     * can be done in InitialiseDaughterCell().
     *
     * @param rModel the cell cycle model to copy.
     */
    HeCellCycleModel(const HeCellCycleModel& rModel);

public:

    /**
     * Constructor - just a default, mBirthTime is set in the AbstractCellCycleModel class.
     */
    HeCellCycleModel();

    /**
     * SetCellCycleDuration() method to set length of cell cycle
     */
    void SetCellCycleDuration();

    /**
     * Overridden builder method to create new copies of
     * this cell-cycle model.
     *
     * @return new cell-cycle model
     */
    AbstractCellCycleModel* CreateCellCycleModel();

    /**
     * Overridden ResetForDivision() method.
     * Contains general mitotic mode logic
     **/
    void ResetForDivision();

    /**
     * Overridden Initialise() method
     * Used to give an appropriate mCellCycleDuration to cells w/ TiL offsets
     * sets mReadytoDivide to false as appropriate
     * Initialises as transit proliferative type
     **/
    void Initialise();

    /**
     * Overridden InitialiseDaughterCell() method.
     * Used to apply sister-cell time shifting (cell cycle duration, deterministic phase boundaries)
     * Used to implement asymmetric mitotic mode
     * */
    void InitialiseDaughterCell();

    /*Model setup functions for standard He (SetModelParameters) and deterministic alternative (SetDeterministicMode) models
     * Default parameters are from refits of He et al + deterministic alternatives
     * He 2012 params: mitoticModePhase2 = 8, mitoticModePhase3 = 15, p1PP = 1, p1PD = 0, p2PP = .2, p2PD = .4, p3PP = .2, p3PD = 0
     * gammaShift = 4, gammaShape = 2, gammaScale = 1, sisterShift = 1
     */
    void SetModelParameters(double tiLOffset = 0, double mitoticModePhase2 = 8, double mitoticModePhase3 = 15,
                            double phase1PP = 1, double phase1PD = 0, double phase2PP = .2, double phase2PD = .4,
                            double phase3PP = .2, double phase3PD = 0, double gammaShift = 4, double gammaShape = 2,
                            double gammaScale = 1, double sisterShift = 1);
    void SetDeterministicMode(double tiLOffset = 0, double mitoticModePhase2 = 8, double mitoticModePhase3 = 15,
                              double phaseShiftWidth = 1, double gammaShift = 4, double gammaShape = 2,
                              double gammaScale = 1, double sisterShift = 1);
    void SetTimeDependentCycleDuration(double peakRateTime, double increasingSlope, double decreasingSlope);

    //Function to set mKillSpecified = true; marks specified neurons for death and removal from population
    //Intended to help w/ resource consumption for WanSimulator
    void EnableKillSpecified();

    //Functions to enable per-cell mitotic mode logging for mode rate & sequence sampling fixtures
    //Uses singleton logfile
    void EnableModeEventOutput(double eventStart, unsigned seed);
    void EnableSequenceSampler();

    //More detailed debug output. Needs a ColumnDataWriter passed to it
    //Only declare ColumnDataWriter directory, filename, etc; do not set up otherwise
    //Use PassDebugWriter if the writer is already enabled elsewhere (ie. in a Wan stem cell cycle model)
    void EnableModelDebugOutput(boost::shared_ptr<ColumnDataWriter> debugWriter);
    void PassDebugWriter(boost::shared_ptr<ColumnDataWriter> debugWriter, int timeID, std::vector<int> varIDs);

    //Not used, but must be overwritten lest HeCellCycleModels be abstract
    double GetAverageTransitCellCycleTime();
    double GetAverageStemCellCycleTime();

    /**
     * Overridden OutputCellCycleModelParameters() method.
     *
     * @param rParamsFile the file stream to which the parameters are output
     */
    virtual void OutputCellCycleModelParameters(out_stream& rParamsFile);
};

#include "SerializationExportWrapper.hpp"
// Declare identifier for the serializer
CHASTE_CLASS_EXPORT(HeCellCycleModel)

#endif /*HECELLCYCLEMODEL_HPP_*/
