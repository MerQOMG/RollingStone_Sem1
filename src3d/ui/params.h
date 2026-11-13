#pragma once
// Parameter model of the scene editor: every editable key with its Chinese label, unit, kind and validity rules,
// plus the Form that tracks applied versus edited values. Text transport is the tested SceneEditor.
#include "editor.h"
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <vector>
namespace rs3d::gui {
enum class Kind { number,integer,count,flag,choice,autoNumber,range };
struct FieldDef {
    std::string section,group,key,label,unit,help;
    Kind kind=Kind::number;
    std::vector<std::string> labels,tokens; // choice display / stored values
    std::string when;                       // "key=value|value" or "!key=value"
    double lo=-std::numeric_limits<double>::infinity(),hi=std::numeric_limits<double>::infinity();
    bool loOpen=false;
};
struct SectionDef {std::string id,title,blurb;};
const std::vector<FieldDef>&fieldDefs();
const std::vector<SectionDef>&sectionDefs();
const FieldDef*findDef(const std::string&key);
struct Val {std::string a,b;}; // b is the upper end of a range
class Form {
    SceneEditor editor_;
    std::map<std::string,Val> now_,applied_;
    std::map<std::string,std::string> errors_,resolved_;
    std::string formError_,applyError_;
    static Val split(const FieldDef&,const std::string&text);
    static std::string compose(const FieldDef&,const Val&);
    std::string localCheck(const FieldDef&,const Val&)const;
public:
    std::filesystem::path outputRoot,appliedOutputRoot;
    explicit Form(const SceneDraft&applied);
    void load(const SceneDraft&applied);
    void rebase(const SceneDraft&applied); // new applied values, keeping unapplied edits
    Val&value(const std::string&key){return now_[key];}
    const Val&appliedValue(const std::string&key)const{static Val none;auto it=applied_.find(key);return it==applied_.end()?none:it->second;}
    bool pending(const std::string&key)const;
    bool pendingOutput()const{return outputRoot!=appliedOutputRoot;}
    int pendingCount(const std::string&section)const;
    int pendingTotal()const;
    bool visible(const FieldDef&)const;
    bool is(const std::string&key,const std::string&token)const;
    const std::string&error(const std::string&key)const{static std::string none;auto it=errors_.find(key);return it==errors_.end()?none:it->second;}
    void setError(const std::string&key,const std::string&message){errors_[key]=message;}
    void setResolved(const std::string&key,double value);
    std::string resolvedText(const std::string&key)const{auto it=resolved_.find(key);return it==resolved_.end()?std::string("0"):it->second;}
    const std::string&formError()const{return formError_;}
    const std::string&applyError()const{return applyError_;}
    void setApplyError(std::string m){applyError_=std::move(m);}
    void clearApplyError(){applyError_.clear();}
    int errorCount()const{return int(errors_.size())+(formError_.empty()?0:1);}
    void validate();
    bool valid()const{return errors_.empty()&&formError_.empty();}
    void revert();
    void revertSection(const std::string&section);
    SceneDraft draft();
};
}
